#pragma once

// WavetableSet — a set of single-cycle frames (up to 256 per preset), each
// pre-rendered into band-limited mip levels so playback never aliases. Frames
// are 2048 samples; mip m keeps harmonics up to (1024 >> m).

#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace newsynth
{

class WavetableSet
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numMips   = 10;
    static constexpr int maxFrames = 256;

    int getNumFrames() const noexcept { return numFrames; }

    const float* table (int frame, int mip) const noexcept
    {
        return data.data() + ((size_t) frame * (size_t) numMips + (size_t) mip) * (size_t) stride;
    }

    // framePos01 in [0,1] morphs across frames; phase01 in [0,1).
    float sample (float framePos01, int mip, float phase01) const noexcept
    {
        const float ff = framePos01 * (float) (numFrames - 1);
        int f0 = (int) ff;
        if (f0 > numFrames - 1) f0 = numFrames - 1;
        const int f1   = f0 + 1 < numFrames ? f0 + 1 : f0;
        const float ft = ff - (float) f0;

        const float p  = phase01 * (float) tableSize;
        const int i0   = (int) p;
        const float pt = p - (float) i0;

        const float* a = table (f0, mip);
        const float sa = a[i0] + pt * (a[i0 + 1] - a[i0]);
        if (f1 == f0)
            return sa;
        const float* b = table (f1, mip);
        const float sb = b[i0] + pt * (b[i0 + 1] - b[i0]);
        return sa + ft * (sb - sa);
    }

    // Choose the mip whose harmonic content stays below Nyquist for the given
    // per-sample phase increment.
    int mipForIncrement (double phaseIncrement) const noexcept
    {
        if (phaseIncrement <= 0.0)
            return 0;
        const double maxHarm = 0.5 / phaseIncrement;
        int mip = 0;
        while (mip < numMips - 1 && (double) (1024 >> mip) > maxHarm)
            ++mip;
        return mip;
    }

    // Built-in table: 16 frames morphing sine -> triangle -> saw -> square.
    static std::unique_ptr<WavetableSet> createDefault();

    // Build from raw audio (e.g. a Serum-style wavetable .wav): consecutive
    // 2048-sample frames, up to maxFrames of them.
    static std::unique_ptr<WavetableSet> fromAudioData (const float* samples, int numSamples);

private:
    static constexpr int stride = tableSize + 1;   // +1 guard point for interpolation
    void build (const std::vector<std::vector<float>>& rawFrames);

    int numFrames = 0;
    std::vector<float> data;
};

} // namespace newsynth
