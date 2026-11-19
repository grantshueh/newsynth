#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace newsynth;

static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* kDegreeNames[12] = { "Unison", "Min 2nd", "Maj 2nd", "Min 3rd", "Maj 3rd", "4th",
                                        "Tritone", "5th", "Min 6th", "Maj 6th", "Min 7th", "Maj 7th" };

//==============================================================================
NewSynthAudioProcessor::NewSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();

    std::shared_ptr<const WavetableSet> def (WavetableSet::createDefault().release());
    for (int i = 0; i < numOscs; ++i)
    {
        wavetables[(size_t) i] = def;
        engine.setWavetable (i, def);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NewSynthAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // oscillators
    for (int i = 0; i < numOscs; ++i)
    {
        const auto pfx  = "osc" + String (i + 1);
        const auto name = "Osc " + String (i + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { pfx + "_on", 1 },   name + "On", i == 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pfx + "_mode", 1 }, name + "Mode",
                                                            StringArray { "Wavetable", "Noise" }, 0));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { pfx + "_pos", 1 },  name + "WT Position",
                                                            NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<AudioParameterInt>    (ParameterID { pfx + "_uni", 1 },  name + "Unison", 1, maxUnison, 1));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { pfx + "_det", 1 },  name + "Unison Detune",
                                                            NormalisableRange<float> (0.0f, 100.0f), 12.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { pfx + "_lvl", 1 },  name + "Level",
                                                            NormalisableRange<float> (0.0f, 1.0f), i == 0 ? 0.8f : 0.5f));
    }

    // amp envelope + globals
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "attack", 1 },  "Attack",
                NormalisableRange<float> (0.001f, 5.0f, 0.0f, 0.35f), 0.005f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "decay", 1 },   "Decay",
                NormalisableRange<float> (0.001f, 5.0f, 0.0f, 0.35f), 0.08f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain",
                NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
                NormalisableRange<float> (0.005f, 8.0f, 0.0f, 0.35f), 0.25f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "master", 1 },  "Master",
                NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "glide", 1 },   "Tuning Glide",
                NormalisableRange<float> (0.0f, 2000.0f, 0.0f, 0.4f), 60.0f));

    // JI ratios for the 12 degrees
    const auto& defs = TuningEngine::defaultRatios();
    for (int d = 0; d < 12; ++d)
    {
        const auto ds = String (d);
        const auto dn = String (kDegreeNames[d]) + " ";
        layout.add (std::make_unique<AudioParameterInt>   (ParameterID { "num" + ds, 1 },  dn + "Numerator",   1, 99, defs[(size_t) d].num));
        layout.add (std::make_unique<AudioParameterInt>   (ParameterID { "den" + ds, 1 },  dn + "Denominator", 1, 99, defs[(size_t) d].den));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "cent" + ds, 1 }, dn + "Cents",
                    NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
    }

    // root / tuning behaviour
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rootSource", 1 }, "Root Source",
                StringArray { "Auto", "Manual" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "detectMode", 1 }, "Root Detect",
                StringArray { "Lowest Note", "Most Played" }, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "noteFromTop", 1 }, "Note From Top", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "manualRoot", 1 }, "Root Note",
                StringArray (kNoteNames, 12), 0));
    layout.add (std::make_unique<AudioParameterInt>    (ParameterID { "notesToTrigger", 1 }, "Notes To Trigger", 1, 8, 2));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "pbRange", 1 }, "Pitchbend Range",
                NormalisableRange<float> (0.0f, 48.0f, 1.0f), 2.0f));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "singleNotePB", 1 }, "Single Note Pitchbend", false));

    return layout;
}

//==============================================================================
void NewSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    monoBuffer.assign ((size_t) std::max (16, samplesPerBlock), 0.0f);
    for (int i = 0; i < numOscs; ++i)
        engine.setWavetable (i, wavetables[(size_t) i]);
}

bool NewSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void NewSynthAudioProcessor::updateEngine()
{
    EngineParams ep;
    for (int i = 0; i < numOscs; ++i)
    {
        const auto pfx = "osc" + juce::String (i + 1);
        auto& op = ep.osc[i];
        op.on          = apvts.getRawParameterValue (pfx + "_on")->load() > 0.5f;
        op.mode        = (int) apvts.getRawParameterValue (pfx + "_mode")->load();
        op.position    = apvts.getRawParameterValue (pfx + "_pos")->load();
        op.unison      = (int) apvts.getRawParameterValue (pfx + "_uni")->load();
        op.detuneCents = apvts.getRawParameterValue (pfx + "_det")->load();
        op.level       = apvts.getRawParameterValue (pfx + "_lvl")->load();
    }
    ep.attack  = p ("attack").load();
    ep.decay   = p ("decay").load();
    ep.sustain = p ("sustain").load();
    ep.release = p ("release").load();
    ep.glideMs = p ("glide").load();
    engine.setParams (ep);

    auto& t = engine.tuning;
    for (int d = 0; d < 12; ++d)
    {
        const auto ds = juce::String (d);
        t.setRatio (d, (int) apvts.getRawParameterValue ("num" + ds)->load(),
                       (int) apvts.getRawParameterValue ("den" + ds)->load());
        t.setCents (d, (double) apvts.getRawParameterValue ("cent" + ds)->load());
    }
    t.setRootSource ((TuningEngine::RootSource) (int) p ("rootSource").load());
    t.setDetectMode ((TuningEngine::DetectMode) (int) p ("detectMode").load());
    t.setNoteFromTop (p ("noteFromTop").load() > 0.5f);
    t.setManualRoot ((int) p ("manualRoot").load());
    t.setNotesToTrigger ((int) p ("notesToTrigger").load());

    engine.setSingleNotePitchbend (p ("singleNotePB").load() > 0.5f);
    engine.setPitchBendSemis (lastBendNorm * p ("pbRange").load());
}

void NewSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;
    if ((int) monoBuffer.size() < n)
        monoBuffer.assign ((size_t) n, 0.0f);

    updateEngine();

    float* mono = monoBuffer.data();
    std::fill (mono, mono + n, 0.0f);

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int t = juce::jlimit (0, n, meta.samplePosition);
        if (t > pos)
        {
            engine.render (mono + pos, t - pos);
            pos = t;
        }

        if (msg.isNoteOn())
            engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            engine.noteOff (msg.getNoteNumber());
        else if (msg.isPitchWheel())
        {
            lastBendNorm = ((float) msg.getPitchWheelValue() - 8192.0f) / 8192.0f;
            engine.setPitchBendSemis (lastBendNorm * p ("pbRange").load());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            engine.allNotesOff();
        }
    }
    if (pos < n)
        engine.render (mono + pos, n - pos);

    currentRoot.store (engine.tuning.getRootPc());

    const float master = p ("master").load();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* dst = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            dst[i] = mono[i] * master;
    }
    midi.clear();
}

//==============================================================================
void NewSynthAudioProcessor::loadWavetableFile (int osc, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return;

    const int len = (int) std::min<juce::int64> (reader->lengthInSamples,
                                                 (juce::int64) WavetableSet::maxFrames * WavetableSet::tableSize);
    if (len <= 0)
        return;

    juce::AudioBuffer<float> buf (1, len);
    reader->read (&buf, 0, len, 0, true, false);

    std::shared_ptr<const WavetableSet> set (WavetableSet::fromAudioData (buf.getReadPointer (0), len).release());
    if (set == nullptr)
        return;

    suspendProcessing (true);
    engine.setWavetable (osc, set);
    wavetables[(size_t) osc] = set;
    wtPaths[(size_t) osc] = file.getFullPathName();
    suspendProcessing (false);
}

void NewSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = 0; i < numOscs; ++i)
        if (wtPaths[(size_t) i].isNotEmpty())
            state.setProperty ("wtPath" + juce::String (i), wtPaths[(size_t) i], nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NewSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            apvts.replaceState (state);
            for (int i = 0; i < numOscs; ++i)
            {
                const auto path = state.getProperty ("wtPath" + juce::String (i), {}).toString();
                if (path.isNotEmpty())
                {
                    juce::File f (path);
                    if (f.existsAsFile())
                        loadWavetableFile (i, f);
                }
            }
        }
    }
}

juce::AudioProcessorEditor* NewSynthAudioProcessor::createEditor()
{
    return new NewSynthAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewSynthAudioProcessor();
}
