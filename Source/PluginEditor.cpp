#include "PluginEditor.h"

static const char* kNoteNamesUI[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

//==============================================================================
// RatioBarsComponent
//==============================================================================
RatioBarsComponent::RatioBarsComponent (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    for (int d = 0; d < 12; ++d)
    {
        for (auto* l : { &numLabels[d], &denLabels[d] })
        {
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::FontOptions (13.0f, juce::Font::bold));
            l->setColour (juce::Label::textColourId, ui::green);
            l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            l->setEditable (true);
            l->addListener (this);
            addAndMakeVisible (*l);
        }
    }
    refreshLabels();
    startTimerHz (6);
}

RatioBarsComponent::~RatioBarsComponent()
{
    for (int d = 0; d < 12; ++d)
    {
        numLabels[d].removeListener (this);
        denLabels[d].removeListener (this);
    }
}

juce::Rectangle<int> RatioBarsComponent::barArea() const
{
    return getLocalBounds().reduced (6).withTrimmedTop (22).withTrimmedBottom (34);
}

int RatioBarsComponent::columnAt (int x) const
{
    const auto b = getLocalBounds().reduced (6);
    const float w = (float) b.getWidth() / 12.0f;
    return juce::jlimit (0, 11, (int) ((float) (x - b.getX()) / w));
}

void RatioBarsComponent::resized()
{
    const auto b = getLocalBounds().reduced (6);
    const float w = (float) b.getWidth() / 12.0f;
    for (int d = 0; d < 12; ++d)
    {
        const int x = b.getX() + (int) (w * (float) d);
        numLabels[d].setBounds (x, b.getY(), (int) w, 20);
        denLabels[d].setBounds (x, b.getBottom() - 32, (int) w, 20);
    }
}

void RatioBarsComponent::paint (juce::Graphics& g)
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (ui::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);

    const auto area = barArea();
    const auto b = getLocalBounds().reduced (6);
    const float w = (float) b.getWidth() / 12.0f;

    // faint horizontal guides every 200 cents
    g.setColour (ui::outline.withAlpha (0.45f));
    for (int c = 200; c < 1200; c += 200)
    {
        const float y = (float) area.getBottom() - (float) c / 1200.0f * (float) area.getHeight();
        g.drawHorizontalLine ((int) y, (float) area.getX(), (float) area.getRight());
    }

    for (int d = 0; d < 12; ++d)
    {
        const auto ds  = juce::String (d);
        const int num  = (int) apvts.getRawParameterValue ("num" + ds)->load();
        const int den  = (int) apvts.getRawParameterValue ("den" + ds)->load();
        const float ct = apvts.getRawParameterValue ("cent" + ds)->load();

        double cents = 1200.0 * std::log2 ((double) num / (double) den) + (double) ct;
        cents = std::fmod (cents, 1200.0);
        if (cents < 0.0)
            cents += 1200.0;

        const float cx = (float) b.getX() + w * ((float) d + 0.5f);
        const float h  = juce::jmax (3.0f, (float) (cents / 1200.0) * (float) area.getHeight());

        g.setColour (d == dragColumn ? ui::green : ui::green.withAlpha (0.85f));
        g.fillRoundedRectangle (cx - 2.5f, (float) area.getBottom() - h, 5.0f, h, 2.0f);

        // fraction line between numerator and bar
        g.setColour (ui::dimGreen.withAlpha (0.6f));
        g.drawHorizontalLine (area.getY() - 1, cx - 8.0f, cx + 8.0f);

        if (std::abs (ct) > 0.05f)
        {
            g.setColour (ui::dimText);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText ((ct > 0 ? "+" : "") + juce::String (ct, 1) + "c",
                        (int) (cx - w * 0.5f), getHeight() - 14, (int) w, 12,
                        juce::Justification::centred);
        }
    }
}

void RatioBarsComponent::mouseDown (const juce::MouseEvent& e)
{
    if (! barArea().contains (e.getPosition()))
        return;
    dragColumn = columnAt (e.x);
    dragStartCents = apvts.getRawParameterValue ("cent" + juce::String (dragColumn))->load();
}

void RatioBarsComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragColumn < 0)
        return;
    const float cents = juce::jlimit (-100.0f, 100.0f,
                                      dragStartCents - (float) e.getDistanceFromDragStartY() * 0.5f);
    if (auto* param = apvts.getParameter ("cent" + juce::String (dragColumn)))
        param->setValueNotifyingHost (param->convertTo0to1 (cents));
    repaint();
}

void RatioBarsComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! barArea().contains (e.getPosition()))
        return;
    const int col = columnAt (e.x);
    if (auto* param = apvts.getParameter ("cent" + juce::String (col)))
        param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
    repaint();
}

void RatioBarsComponent::labelTextChanged (juce::Label* label)
{
    for (int d = 0; d < 12; ++d)
    {
        const bool isNum = label == &numLabels[d];
        const bool isDen = label == &denLabels[d];
        if (! isNum && ! isDen)
            continue;

        const int v = juce::jlimit (1, 99, label->getText().getIntValue());
        if (auto* param = apvts.getParameter ((isNum ? "num" : "den") + juce::String (d)))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) v));
        label->setText (juce::String (v), juce::dontSendNotification);
        repaint();
        return;
    }
}

void RatioBarsComponent::refreshLabels()
{
    for (int d = 0; d < 12; ++d)
    {
        const auto ds = juce::String (d);
        if (! numLabels[d].isBeingEdited())
            numLabels[d].setText (juce::String ((int) apvts.getRawParameterValue ("num" + ds)->load()),
                                  juce::dontSendNotification);
        if (! denLabels[d].isBeingEdited())
            denLabels[d].setText (juce::String ((int) apvts.getRawParameterValue ("den" + ds)->load()),
                                  juce::dontSendNotification);
    }
}

void RatioBarsComponent::timerCallback()
{
    refreshLabels();
    repaint();
}

//==============================================================================
// WaveformDisplay
//==============================================================================
void WaveformDisplay::paint (juce::Graphics& g)
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (ui::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);

    auto table = proc.getWavetable (selected);
    if (table == nullptr)
        return;

    const auto pfx = "osc" + juce::String (selected + 1);
    const float pos = proc.apvts.getRawParameterValue (pfx + "_pos")->load();

    const auto r = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
    const int points = 256;

    juce::Path path;
    for (int i = 0; i < points; ++i)
    {
        const float ph = (float) i / (float) points;
        const float v  = table->sample (pos, 0, ph);
        const float x  = r.getX() + ph * r.getWidth();
        const float y  = r.getCentreY() - v * r.getHeight() * 0.48f;
        if (i == 0) path.startNewSubPath (x, y);
        else        path.lineTo (x, y);
    }

    g.setColour (ui::dimGreen.withAlpha (0.25f));
    juce::Path fill (path);
    fill.lineTo (r.getRight(), r.getCentreY());
    fill.lineTo (r.getX(), r.getCentreY());
    fill.closeSubPath();
    g.fillPath (fill);

    g.setColour (ui::green);
    g.strokePath (path, juce::PathStrokeType (1.6f));

    const int frames = table->getNumFrames();
    const int frame  = 1 + (int) std::round (pos * (float) (frames - 1));
    g.setColour (ui::dimText);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("OSC " + juce::String (selected + 1) + "   FRAME " + juce::String (frame) + "/" + juce::String (frames),
                getLocalBounds().reduced (10, 4), juce::Justification::bottomLeft);
}

//==============================================================================
// Editor
//==============================================================================
NewSynthAudioProcessorEditor::NewSynthAudioProcessorEditor (NewSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveform (p, selectedOsc), ratioBars (p.apvts)
{
    // --- look & feel ---------------------------------------------------------
    lnf.setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    lnf.setColour (juce::Slider::rotarySliderFillColourId, ui::green);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, ui::outline);
    lnf.setColour (juce::Slider::thumbColourId, ui::green);
    lnf.setColour (juce::Slider::trackColourId, ui::dimGreen);
    lnf.setColour (juce::Slider::backgroundColourId, ui::outline);
    lnf.setColour (juce::Slider::textBoxTextColourId, ui::text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::Label::textColourId, ui::text);
    lnf.setColour (juce::ToggleButton::textColourId, ui::text);
    lnf.setColour (juce::ToggleButton::tickColourId, ui::green);
    lnf.setColour (juce::ToggleButton::tickDisabledColourId, ui::dimText);
    lnf.setColour (juce::ComboBox::backgroundColourId, ui::panel);
    lnf.setColour (juce::ComboBox::textColourId, ui::text);
    lnf.setColour (juce::ComboBox::outlineColourId, ui::outline);
    lnf.setColour (juce::ComboBox::arrowColourId, ui::green);
    lnf.setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    lnf.setColour (juce::PopupMenu::textColourId, ui::text);
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::dimGreen);
    lnf.setColour (juce::TextButton::buttonColourId, ui::panel);
    lnf.setColour (juce::TextButton::buttonOnColourId, ui::dimGreen);
    lnf.setColour (juce::TextButton::textColourOffId, ui::text);
    lnf.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    lnf.setColour (juce::TextEditor::backgroundColourId, ui::bg);
    lnf.setColour (juce::TextEditor::textColourId, ui::green);
    lnf.setColour (juce::TextEditor::highlightColourId, ui::dimGreen);
    lnf.setColour (juce::TextEditor::focusedOutlineColourId, ui::green);
    setLookAndFeel (&lnf);

    // --- header --------------------------------------------------------------
    rootLabel.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    rootLabel.setColour (juce::Label::textColourId, ui::green);
    rootLabel.setText ("ROOT  C", juce::dontSendNotification);
    addAndMakeVisible (rootLabel);

    titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, ui::dimText);
    titleLabel.setJustificationType (juce::Justification::centredRight);
    titleLabel.setText ("N E W S Y N T H   —   DYNAMIC JUST INTONATION", juce::dontSendNotification);
    addAndMakeVisible (titleLabel);

    // --- waveform + osc view buttons ------------------------------------------
    addAndMakeVisible (waveform);
    for (int i = 0; i < newsynth::numOscs; ++i)
    {
        auto& b = oscViewButtons[i];
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { selectOsc (i); };
        addAndMakeVisible (b);
    }

    // --- oscillator strips -----------------------------------------------------
    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (10.0f));
        l.setColour (juce::Label::textColourId, ui::dimText);
        addAndMakeVisible (l);
    };

    for (int i = 0; i < newsynth::numOscs; ++i)
    {
        auto& st = strips[i];
        const auto pfx = "osc" + juce::String (i + 1);

        st.on.setButtonText ("OSC " + juce::String (i + 1));
        addAndMakeVisible (st.on);
        buttonAtts.push_back (std::make_unique<ButtonAttachment> (proc.apvts, pfx + "_on", st.on));

        st.mode.addItemList ({ "Wavetable", "Noise" }, 1);
        addAndMakeVisible (st.mode);
        comboAtts.push_back (std::make_unique<ComboBoxAttachment> (proc.apvts, pfx + "_mode", st.mode));

        setupKnob (st.pos, st.posL, "POS");
        setupKnob (st.uni, st.uniL, "UNI");
        setupKnob (st.det, st.detL, "DET");
        setupKnob (st.lvl, st.lvlL, "LVL");
        sliderAtts.push_back (std::make_unique<SliderAttachment> (proc.apvts, pfx + "_pos", st.pos));
        sliderAtts.push_back (std::make_unique<SliderAttachment> (proc.apvts, pfx + "_uni", st.uni));
        sliderAtts.push_back (std::make_unique<SliderAttachment> (proc.apvts, pfx + "_det", st.det));
        sliderAtts.push_back (std::make_unique<SliderAttachment> (proc.apvts, pfx + "_lvl", st.lvl));

        st.load.onClick = [this, i]
        {
            selectOsc (i);
            chooser = std::make_unique<juce::FileChooser> ("Load wavetable (2048-sample frames)",
                                                           juce::File(), "*.wav;*.aif;*.aiff");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this, i] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile())
                                      {
                                          proc.loadWavetableFile (i, f);
                                          waveform.repaint();
                                      }
                                  });
        };
        addAndMakeVisible (st.load);
    }

    // --- envelope ---------------------------------------------------------------
    const char* adsrIds[4]   = { "attack", "decay", "sustain", "release" };
    const char* adsrNames[4] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        auto& s = adsrSliders[i];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (s);
        sliderAtts.push_back (std::make_unique<SliderAttachment> (proc.apvts, adsrIds[i], s));

        adsrLabels[i].setText (adsrNames[i], juce::dontSendNotification);
        adsrLabels[i].setJustificationType (juce::Justification::centred);
        adsrLabels[i].setFont (juce::FontOptions (11.0f, juce::Font::bold));
        adsrLabels[i].setColour (juce::Label::textColourId, ui::dimText);
        addAndMakeVisible (adsrLabels[i]);
    }

    // --- ratio bars ---------------------------------------------------------------
    ratioTitle.setText ("JI RATIOS — 12 DEGREES FROM ROOT", juce::dontSendNotification);
    ratioTitle.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    ratioTitle.setColour (juce::Label::textColourId, ui::dimText);
    addAndMakeVisible (ratioTitle);
    addAndMakeVisible (ratioBars);

    // --- bottom row ------------------------------------------------------------
    rootSourceBox.addItemList ({ "Auto", "Manual" }, 1);
    addAndMakeVisible (rootSourceBox);
    comboAtts.push_back (std::make_unique<ComboBoxAttachment> (proc.apvts, "rootSource", rootSourceBox));

    detectModeBox.addItemList ({ "Lowest Note", "Most Played" }, 1);
    addAndMakeVisible (detectModeBox);
    comboAtts.push_back (std::make_unique<ComboBoxAttachment> (proc.apvts, "detectMode", detectModeBox));

    auto setupBigKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const char* id)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 15);
        addAndMakeVisible (s);
        sliderAtts.push_back (std::make_unique<SliderAttachment> (proc.apvts, id, s));
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, ui::dimText);
        addAndMakeVisible (l);
    };
    setupBigKnob (pbRangeKnob,   pbRangeL,   "PITCHBEND RANGE", "pbRange");
    setupBigKnob (notesTrigKnob, notesTrigL, "NOTES TO TRIGGER", "notesToTrigger");
    setupBigKnob (rootNoteKnob,  rootNoteL,  "ROOT NOTE", "manualRoot");
    setupBigKnob (glideKnob,     glideL,     "GLIDE MS", "glide");
    setupBigKnob (masterKnob,    masterL,    "MASTER", "master");

    addAndMakeVisible (singleNotePBToggle);
    buttonAtts.push_back (std::make_unique<ButtonAttachment> (proc.apvts, "singleNotePB", singleNotePBToggle));
    addAndMakeVisible (noteFromTopToggle);
    buttonAtts.push_back (std::make_unique<ButtonAttachment> (proc.apvts, "noteFromTop", noteFromTopToggle));

    selectOsc (0);
    setSize (960, 640);
    startTimerHz (15);
}

NewSynthAudioProcessorEditor::~NewSynthAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void NewSynthAudioProcessorEditor::selectOsc (int index)
{
    selectedOsc = index;
    for (int i = 0; i < newsynth::numOscs; ++i)
    {
        oscViewButtons[i].setColour (juce::TextButton::buttonColourId, i == index ? ui::dimGreen : ui::panel);
        oscViewButtons[i].setColour (juce::TextButton::textColourOffId, i == index ? juce::Colours::black : ui::text);
    }
    waveform.repaint();
}

void NewSynthAudioProcessorEditor::timerCallback()
{
    rootLabel.setText ("ROOT  " + juce::String (kNoteNamesUI[proc.getCurrentRootPc() % 12]),
                       juce::dontSendNotification);
    waveform.repaint();
}

void NewSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::outline.withAlpha (0.6f));
    g.drawHorizontalLine (46, 12.0f, (float) getWidth() - 12.0f);
    g.drawHorizontalLine (getHeight() - 104, 12.0f, (float) getWidth() - 12.0f);
}

void NewSynthAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // header
    auto header = r.removeFromTop (46).reduced (14, 6);
    rootLabel.setBounds (header.removeFromLeft (220));
    titleLabel.setBounds (header);

    // bottom row
    auto bottom = r.removeFromBottom (104).reduced (14, 8);
    {
        auto combos = bottom.removeFromLeft (120);
        rootSourceBox.setBounds (combos.removeFromTop (26));
        combos.removeFromTop (8);
        detectModeBox.setBounds (combos.removeFromTop (26));

        bottom.removeFromLeft (10);
        auto toggles = bottom.removeFromRight (190);
        singleNotePBToggle.setBounds (toggles.removeFromTop (26));
        toggles.removeFromTop (8);
        noteFromTopToggle.setBounds (toggles.removeFromTop (26));

        juce::Slider* knobs[5]  = { &pbRangeKnob, &notesTrigKnob, &rootNoteKnob, &glideKnob, &masterKnob };
        juce::Label* kLabels[5] = { &pbRangeL, &notesTrigL, &rootNoteL, &glideL, &masterL };
        const int kw = bottom.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
        {
            auto cell = bottom.removeFromLeft (kw);
            kLabels[i]->setBounds (cell.removeFromTop (12));
            knobs[i]->setBounds (cell.reduced (4, 0));
        }
    }

    r.reduce (14, 8);

    // right column: ratio bars
    auto right = r.removeFromRight (440);
    ratioTitle.setBounds (right.removeFromTop (16));
    ratioBars.setBounds (right);

    r.removeFromRight (12);

    // left column
    auto wf = r.removeFromTop (140);
    waveform.setBounds (wf);
    for (int i = 0; i < newsynth::numOscs; ++i)
        oscViewButtons[i].setBounds (wf.getRight() - 26 * (newsynth::numOscs - i) - 6,
                                     wf.getY() + 6, 22, 18);

    r.removeFromTop (8);

    // envelope block at the bottom of the left column
    auto env = r.removeFromBottom (96);
    const int ew = env.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = env.removeFromLeft (ew);
        adsrLabels[i].setBounds (cell.removeFromBottom (14));
        adsrSliders[i].setBounds (cell.reduced (juce::jmax (4, ew / 2 - 14), 2));
    }

    // oscillator strips
    const int rowH = r.getHeight() / newsynth::numOscs;
    for (int i = 0; i < newsynth::numOscs; ++i)
    {
        auto row = r.removeFromTop (rowH).reduced (0, 4);
        auto& st = strips[i];
        st.on.setBounds (row.removeFromLeft (66));
        st.mode.setBounds (row.removeFromLeft (92).reduced (0, (row.getHeight() - 24) / 2));
        row.removeFromLeft (4);
        st.load.setBounds (row.removeFromRight (48).reduced (0, (row.getHeight() - 22) / 2));

        juce::Slider* knobs[4]  = { &st.pos, &st.uni, &st.det, &st.lvl };
        juce::Label* kLabels[4] = { &st.posL, &st.uniL, &st.detL, &st.lvlL };
        const int kw = row.getWidth() / 4;
        for (int k = 0; k < 4; ++k)
        {
            auto cell = row.removeFromLeft (kw);
            kLabels[k]->setBounds (cell.removeFromBottom (11));
            knobs[k]->setBounds (cell);
        }
    }
}
