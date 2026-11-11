#include "PluginEditor.h"

namespace
{
    const juce::Colour background { 0xff0e0d12 };
    const juce::Colour panel { 0xff191720 };
    const juce::Colour panelLight { 0xff24212d };
    const juce::Colour accent { 0xffb879ff };
    const juce::Colour tuned { 0xff66c7ff };
    const juce::Colour mainText { 0xfff5efff };
    const juce::Colour mutedText { 0xff91899e };
    const juce::Colour outline { 0xff393442 };
}

AntihumanScaleAudioProcessorEditor::AntihumanScaleAudioProcessorEditor(
    AntihumanScaleAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    setSize(650, 440);

    rootBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleBox.addItemList({ "Major", "Natural Minor", "Major Pentatonic", "Minor Pentatonic",
                           "Harmonic Minor", "Dorian", "Chromatic" }, 1);

    for (auto* box : { &rootBox, &scaleBox })
    {
        box->setColour(juce::ComboBox::backgroundColourId, panelLight);
        box->setColour(juce::ComboBox::textColourId, mainText);
        box->setColour(juce::ComboBox::outlineColourId, outline);
        box->setColour(juce::ComboBox::arrowColourId, accent);
        addAndMakeVisible(*box);
    }

    midiButton.setColour(juce::ToggleButton::textColourId, mainText);
    midiButton.setColour(juce::ToggleButton::tickColourId, tuned);
    midiButton.setColour(juce::ToggleButton::tickDisabledColourId, mutedText);
    addAndMakeVisible(midiButton);

    setupSlider(amountSlider);
    setupSlider(mixSlider);
    setupSlider(colourSlider);
    setupSlider(morphSlider);
    setupSlider(gateSlider);
    setupSlider(pitchSlider);

    amountSlider.setRange(0.0, 100.0, 0.1);
    mixSlider.setRange(0.0, 100.0, 0.1);
    colourSlider.setRange(0.0, 100.0, 0.1);
    morphSlider.setRange(0.0, 100.0, 0.1);
    gateSlider.setRange(0.0, 100.0, 0.1);
    pitchSlider.setRange(-12.0, 12.0, 0.01);
    amountSlider.setTextValueSuffix("%");
    mixSlider.setTextValueSuffix("%");
    colourSlider.setTextValueSuffix("%");
    morphSlider.setTextValueSuffix("%");
    gateSlider.setTextValueSuffix("%");
    pitchSlider.setTextValueSuffix(" st");

    setupLabel(rootLabel, "ROOT NOTE");
    setupLabel(scaleLabel, "SCALE");
    setupLabel(amountLabel, "AMOUNT");
    setupLabel(mixLabel, "DRY / WET");
    setupLabel(colourLabel, "COLOUR");
    setupLabel(morphLabel, "MORPH");
    setupLabel(gateLabel, "GATE");
    setupLabel(pitchLabel, "PITCH");

    rootAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.parameters, "root", rootBox);
    scaleAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.parameters, "scale", scaleBox);
    midiAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, "midi", midiButton);
    amountAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, "amount", amountSlider);
    mixAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, "mix", mixSlider);
    colourAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, "colour", colourSlider);
    morphAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, "morph", morphSlider);
    gateAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, "gate", gateSlider);
    pitchAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, "transpose", pitchSlider);

    startTimerHz(30);
}

void AntihumanScaleAudioProcessorEditor::setupSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, outline);
    slider.setColour(juce::Slider::thumbColourId, mainText);
    slider.setColour(juce::Slider::textBoxTextColourId, mainText);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panelLight);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
    addAndMakeVisible(slider);
}

void AntihumanScaleAudioProcessorEditor::setupLabel(juce::Label& label,
                                                     const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, mutedText);
    label.setFont(juce::Font(juce::FontOptions(10.5f).withStyle("Bold")));
    addAndMakeVisible(label);
}

void AntihumanScaleAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);

    auto outer = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(outer, 16.0f);

    g.setColour(accent);
    g.setFont(juce::Font(juce::FontOptions(25.0f).withStyle("Bold")));
    g.drawText("ant!human", 25, 16, 210, 31, juce::Justification::centredLeft);

    g.setColour(mainText);
    g.setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
    g.drawText("SCALE", 25, 48, 120, 20, juce::Justification::centredLeft);

    g.setColour(mutedText);
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.drawText("SPECTRAL PITCH QUANTIZER  /  KEY-AWARE RESHAPING",
               150, 48, 360, 20, juce::Justification::centredLeft);

    g.setColour(outline);
    g.drawHorizontalLine(76, 25.0f, static_cast<float>(getWidth() - 25));

    // Visualizer panel
    const juce::Rectangle<float> visualizer(24.0f, 125.0f,
        static_cast<float>(getWidth() - 48), 100.0f);
    g.setColour(panelLight);
    g.fillRoundedRectangle(visualizer, 9.0f);

    g.setColour(mutedText);
    g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    g.drawText("LIVE SPECTRUM", 35, 129, 130, 14, juce::Justification::centredLeft);
    g.setColour(accent.withAlpha(0.18f));
    g.drawHorizontalLine(218, 34.0f, static_cast<float>(getWidth() - 34));

    const float graphLeft = 36.0f;
    const float graphTop = 151.0f;
    const float graphWidth = static_cast<float>(getWidth()) - 72.0f;
    const float graphHeight = 58.0f;
    const float barGap = 2.0f;
    const float barWidth = (graphWidth - barGap * 63.0f) / 64.0f;

    for (int i = 0; i < 64; ++i)
    {
        const auto value = audioProcessor.getSpectrumPoint(i);
        const auto barHeight = 3.0f + value * graphHeight;
        const auto x = graphLeft + static_cast<float>(i) * (barWidth + barGap);
        const auto y = graphTop + graphHeight - barHeight;
        const auto colour = accent.interpolatedWith(tuned, static_cast<float>(i) / 63.0f);
        g.setColour(colour.withAlpha(0.35f + value * 0.65f));
        g.fillRoundedRectangle(x, y, barWidth, barHeight, 2.0f);
    }

    g.setColour(mutedText.withAlpha(0.75f));
    g.setFont(juce::Font(juce::FontOptions(8.5f)));
    g.drawText("100 Hz", 35, 211, 60, 10, juce::Justification::centredLeft);
    g.drawText("6 kHz", getWidth() - 90, 211, 55, 10, juce::Justification::centredRight);

    g.setColour(outline);
    g.drawHorizontalLine(239, 25.0f, static_cast<float>(getWidth() - 25));

    g.setColour(mutedText);
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("Amount snaps partials to the selected scale. MIDI ROOT follows incoming note-ons.",
               25, getHeight() - 19, getWidth() - 50, 12, juce::Justification::centredLeft);
}

void AntihumanScaleAudioProcessorEditor::resized()
{
    rootLabel.setBounds(26, 79, 90, 18);
    rootBox.setBounds(26, 94, 92, 28);
    scaleLabel.setBounds(132, 79, 90, 18);
    scaleBox.setBounds(132, 94, 190, 28);
    midiButton.setBounds(455, 91, 145, 30);

    const int left = 20;
    const int totalWidth = getWidth() - 40;
    const int columnWidth = totalWidth / 6;
    const int labelY = 251;
    const int sliderY = 265;
    const int sliderHeight = 145;

    std::array<juce::Label*, 6> labels {
        &amountLabel, &mixLabel, &colourLabel, &morphLabel, &gateLabel, &pitchLabel
    };
    std::array<juce::Slider*, 6> sliders {
        &amountSlider, &mixSlider, &colourSlider, &morphSlider, &gateSlider, &pitchSlider
    };

    for (int i = 0; i < 6; ++i)
    {
        const int x = left + i * columnWidth;
        labels[static_cast<size_t>(i)]->setBounds(x, labelY, columnWidth, 18);
        sliders[static_cast<size_t>(i)]->setBounds(x + 2, sliderY, columnWidth - 4, sliderHeight);
    }
}

void AntihumanScaleAudioProcessorEditor::timerCallback()
{
    repaint(24, 125, getWidth() - 48, 100);
}
