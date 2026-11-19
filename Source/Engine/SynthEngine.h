#pragma once

// SynthEngine — 8-voice polyphonic wavetable engine with per-voice JI tuning.
// Each voice runs 4 oscillators (wavetable or noise), each with up to 16
// unison voices. Tuning is applied per voice: targets are re-read from the
// TuningEngine every render segment, and voices glide to new targets when the
// detected root changes.

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdint>
#include <memory>

#include "TuningEngine.h"
#include "Wavetable.h"

namespace newsynth
{

constexpr int maxVoices = 8;
constexpr int numOscs   = 4;
constexpr int maxUnison = 16;

struct OscParams
{
    bool  on          = false;
    int   mode        = 0;      // 0 = wavetable, 1 = noise
    float position    = 0.0f;   // 0..1 across frames
    int   unison      = 1;      // 1..16
    float detuneCents = 12.0f;  // unison spread
    float level       = 0.7f;
};

struct EngineParams
{
    OscParams osc[numOscs];
    float attack = 0.005f, decay = 0.08f, sustain = 0.8f, release = 0.25f;
    float glideMs = 60.0f;
};

//==============================================================================
class Voice
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        adsr.setSampleRate (sampleRate);
        kill();
    }

    void start (int newNote, float velocity, double startFreq, uint64_t stamp)
    {
        note = newNote;
        vel = velocity;
        freq = targetFreq = startFreq;
        startStamp = stamp;
        for (auto& oscPhases : phases)
            for (auto& p : oscPhases)
                p = nextRand01();
        adsr.reset();
        adsr.noteOn();
        active = true;
        releasing = false;
    }

    void stop()
    {
        if (active)
        {
            adsr.noteOff();
            releasing = true;
        }
    }

    void kill()
    {
        active = false;
        releasing = false;
        note = -1;
        adsr.reset();
    }

    bool isActive() const noexcept     { return active; }
    bool isReleasing() const noexcept  { return releasing; }
    int currentNote() const noexcept   { return note; }
    uint64_t stamp() const noexcept    { return startStamp; }

    void setTargetFrequency (double f) { targetFreq = f; }

    void render (float* out, int n, const EngineParams& p, const WavetableSet* const* tables)
    {
        if (! active || n <= 0)
            return;

        adsr.setParameters ({ p.attack, p.decay, p.sustain, p.release });

        const double glideSec = (double) p.glideMs * 0.001;
        const double gc = glideSec < 1.0e-4 ? 1.0
                                            : 1.0 - std::exp (-1.0 / (sr * glideSec));

        struct OscBlock
        {
            bool on = false, noise = false;
            int unison = 1, mip = 0;
            float level = 0.0f, pos = 0.0f, ugain = 1.0f;
            double detFactor[maxUnison];
            const WavetableSet* tbl = nullptr;
        };
        OscBlock ob[numOscs];

        for (int o = 0; o < numOscs; ++o)
        {
            const auto& op = p.osc[o];
            auto& b = ob[o];
            b.on = op.on;
            if (! b.on)
                continue;
            b.noise  = op.mode == 1;
            b.level  = op.level;
            b.pos    = juce::jlimit (0.0f, 1.0f, op.position);
            b.unison = juce::jlimit (1, maxUnison, op.unison);
            b.ugain  = 1.0f / std::sqrt ((float) b.unison);
            b.tbl    = tables[o];

            double maxFactor = 1.0;
            for (int u = 0; u < b.unison; ++u)
            {
                const double spread = b.unison == 1 ? 0.0
                                      : ((double) u / (double) (b.unison - 1)) * 2.0 - 1.0;
                b.detFactor[u] = std::exp2 ((double) op.detuneCents * spread / 1200.0);
                maxFactor = std::max (maxFactor, b.detFactor[u]);
            }
            if (b.tbl != nullptr)
                b.mip = b.tbl->mipForIncrement (std::max (freq, targetFreq) * maxFactor / sr);
        }

        for (int s = 0; s < n; ++s)
        {
            freq += (targetFreq - freq) * gc;
            const float env = adsr.getNextSample();
            if (! adsr.isActive())
            {
                kill();
                return;
            }

            float sum = 0.0f;
            for (int o = 0; o < numOscs; ++o)
            {
                const auto& b = ob[o];
                if (! b.on)
                    continue;

                if (b.noise)
                {
                    sum += b.level * nextNoise();
                    continue;
                }
                if (b.tbl == nullptr)
                    continue;

                const double inc = freq / sr;
                float oscSum = 0.0f;
                for (int u = 0; u < b.unison; ++u)
                {
                    double ph = phases[o][(size_t) u] + inc * b.detFactor[u];
                    ph -= std::floor (ph);
                    phases[o][(size_t) u] = ph;
                    oscSum += b.tbl->sample (b.pos, b.mip, (float) ph);
                }
                sum += oscSum * b.level * b.ugain;
            }
            out[s] += sum * env * vel;
        }
    }

private:
    float nextNoise() noexcept
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        return (float) (int32_t) noiseState * (1.0f / 2147483648.0f);
    }
    double nextRand01() noexcept
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        return (double) noiseState / 4294967296.0;
    }

    double sr = 44100.0;
    int note = -1;
    float vel = 0.0f;
    bool active = false, releasing = false;
    uint64_t startStamp = 0;
    double freq = 440.0, targetFreq = 440.0;
    std::array<std::array<double, maxUnison>, numOscs> phases {};
    uint32_t noiseState = 0x12345678u;
    juce::ADSR adsr;
};

//==============================================================================
class SynthEngine
{
public:
    TuningEngine tuning;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices)
            v.prepare (sampleRate);
        tuning.reset();
        bendSemis = 0.0f;
        stamp = 0;
    }

    void setParams (const EngineParams& p)                              { params = p; }
    void setWavetable (int osc, std::shared_ptr<const WavetableSet> t)  { tables[(size_t) osc] = std::move (t); }
    void setPitchBendSemis (float semitones)                            { bendSemis = semitones; }
    void setSingleNotePitchbend (bool b)                                { singleNotePB = b; }

    void noteOn (int note, float vel)
    {
        tuning.noteOn (note);
        Voice& v = allocateVoice (note);
        v.start (note, vel, tuning.frequencyForNote (note) * currentBendFactor(), ++stamp);
    }

    void noteOff (int note)
    {
        tuning.noteOff (note);
        for (auto& v : voices)
            if (v.isActive() && ! v.isReleasing() && v.currentNote() == note)
            {
                v.stop();
                break;
            }
    }

    void allNotesOff()
    {
        tuning.reset();
        for (auto& v : voices)
            v.stop();
    }

    void render (float* out, int n)
    {
        const double bf = currentBendFactor();
        const WavetableSet* t[numOscs];
        for (int i = 0; i < numOscs; ++i)
            t[i] = tables[(size_t) i].get();

        for (auto& v : voices)
        {
            if (! v.isActive())
                continue;
            v.setTargetFrequency (tuning.frequencyForNote (v.currentNote()) * bf);
            v.render (out, n, params, t);
        }
    }

private:
    double currentBendFactor() const
    {
        if (singleNotePB && tuning.heldCount() > 1)
            return 1.0;
        return std::exp2 ((double) bendSemis / 12.0);
    }

    Voice& allocateVoice (int note)
    {
        // retrigger an existing voice on the same note
        for (auto& v : voices)
            if (v.isActive() && v.currentNote() == note && ! v.isReleasing())
                return v;
        // otherwise a free voice
        for (auto& v : voices)
            if (! v.isActive())
                return v;
        // otherwise steal the oldest
        Voice* oldest = &voices[0];
        for (auto& v : voices)
            if (v.stamp() < oldest->stamp())
                oldest = &v;
        return *oldest;
    }

    double sr = 44100.0;
    Voice voices[maxVoices];
    std::shared_ptr<const WavetableSet> tables[numOscs];
    EngineParams params;
    float bendSemis = 0.0f;
    bool singleNotePB = false;
    uint64_t stamp = 0;
};

} // namespace newsynth
