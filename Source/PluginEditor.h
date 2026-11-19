#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg       { 0xff0b0d0b };
const juce::Colour panel    { 0xff121712 };
const juce::Colour outline  { 0xff24352a };
const juce::Colour green    { 0xff46e07d };
const juce::Colour dimGreen { 0xff2d8f52 };
const juce::Colour text     { 0xffc9d4c9 };
const juce::Colour dimText  { 0xff76856f };
}

//==============================================================================
// The 12 JI ratio bars: numerator above, thin green bar, denominator below.
// Click a number to edit it; drag vertically on a bar to adjust its cent
// offset; double-click a bar to reset the offset.
class RatioBarsComponent : public juce::Component,
                           private juce::Timer,
                           private juce::Label::Listener
{
public:
    explicit RatioBarsComponent (juce::AudioProcessorValueTreeState& state);
    ~RatioBarsComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void labelTextChanged (juce::Label*) override;
    void refreshLabels();
    int columnAt (int x) const;
    juce::Rectangle<int> barArea() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::Label numLabels[12], denLabels[12];
    int dragColumn = -1;
    float dragStartCents = 0.0f;
};

//==============================================================================
// Draws the current frame of the selected oscillator's wavetable.
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay (NewSynthAudioProcessor& p, const int& selectedOsc)
        : proc (p), selected (selectedOsc) {}
    void paint (juce::Graphics&) override;

private:
    NewSynthAudioProcessor& proc;
    const int& selected;
};

//==============================================================================
class NewSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NewSynthAudioProcessorEditor (NewSynthAudioProcessor&);
    ~NewSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectOsc (int index);

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    NewSynthAudioProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    // header
    juce::Label rootLabel, titleLabel;

    // waveform + osc selection
    int selectedOsc = 0;
    WaveformDisplay waveform;
    juce::TextButton oscViewButtons[newsynth::numOscs];

    // oscillator strips
    struct OscStrip
    {
        juce::ToggleButton on;
        juce::ComboBox mode;
        juce::Slider pos, uni, det, lvl;
        juce::Label posL, uniL, detL, lvlL;
        juce::TextButton load { "LOAD" };
    };
    OscStrip strips[newsynth::numOscs];

    // envelope
    juce::Slider adsrSliders[4];
    juce::Label adsrLabels[4];

    // ratio bars
    RatioBarsComponent ratioBars;
    juce::Label ratioTitle;

    // bottom row
    juce::ComboBox rootSourceBox, detectModeBox;
    juce::Slider pbRangeKnob, notesTrigKnob, rootNoteKnob, glideKnob, masterKnob;
    juce::Label pbRangeL, notesTrigL, rootNoteL, glideL, masterL;
    juce::ToggleButton singleNotePBToggle { "SINGLE NOTE PITCHBEND" },
                       noteFromTopToggle  { "NOTE FROM TOP" };

    std::vector<std::unique_ptr<SliderAttachment>>   sliderAtts;
    std::vector<std::unique_ptr<ButtonAttachment>>   buttonAtts;
    std::vector<std::unique_ptr<ComboBoxAttachment>> comboAtts;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewSynthAudioProcessorEditor)
};
