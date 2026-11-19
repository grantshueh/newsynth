#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Engine/SynthEngine.h"

class NewSynthAudioProcessor : public juce::AudioProcessor
{
public:
    NewSynthAudioProcessor();
    ~NewSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                                   { return true; }

    const juce::String getName() const override                       { return JucePlugin_Name; }
    bool acceptsMidi() const override                                 { return true; }
    bool producesMidi() const override                                 { return false; }
    bool isMidiEffect() const override                                 { return false; }
    double getTailLengthSeconds() const override                       { return 0.0; }

    int getNumPrograms() override                                      { return 1; }
    int getCurrentProgram() override                                   { return 0; }
    void setCurrentProgram (int) override                              {}
    const juce::String getProgramName (int) override                   { return "Default"; }
    void changeProgramName (int, const juce::String&) override         {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- UI access -----------------------------------------------------------
    int getCurrentRootPc() const noexcept                              { return currentRoot.load(); }
    std::shared_ptr<const newsynth::WavetableSet> getWavetable (int osc) const
    {
        return wavetables[(size_t) osc];
    }
    void loadWavetableFile (int osc, const juce::File& file);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEngine();
    std::atomic<float>& p (const char* id) const
    {
        return *apvts.getRawParameterValue (id);
    }

    newsynth::SynthEngine engine;
    std::vector<float> monoBuffer;
    std::atomic<int> currentRoot { 0 };
    std::shared_ptr<const newsynth::WavetableSet> wavetables[newsynth::numOscs];
    juce::String wtPaths[newsynth::numOscs];
    juce::AudioFormatManager formatManager;
    float lastBendNorm = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewSynthAudioProcessor)
};
