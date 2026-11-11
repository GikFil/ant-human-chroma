#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AntihumanScaleAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit AntihumanScaleAudioProcessorEditor(AntihumanScaleAudioProcessor&);
    ~AntihumanScaleAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AntihumanScaleAudioProcessor& audioProcessor;

    juce::ComboBox rootBox;
    juce::ComboBox scaleBox;
    juce::ToggleButton midiButton { "MIDI ROOT" };

    juce::Slider amountSlider;
    juce::Slider mixSlider;
    juce::Slider colourSlider;
    juce::Slider morphSlider;
    juce::Slider gateSlider;
    juce::Slider pitchSlider;

    juce::Label rootLabel;
    juce::Label scaleLabel;
    juce::Label amountLabel;
    juce::Label mixLabel;
    juce::Label colourLabel;
    juce::Label morphLabel;
    juce::Label gateLabel;
    juce::Label pitchLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> rootAttachment;
    std::unique_ptr<ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<ButtonAttachment> midiAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> colourAttachment;
    std::unique_ptr<SliderAttachment> morphAttachment;
    std::unique_ptr<SliderAttachment> gateAttachment;
    std::unique_ptr<SliderAttachment> pitchAttachment;

    void setupSlider(juce::Slider& slider);
    void setupLabel(juce::Label& label, const juce::String& text);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AntihumanScaleAudioProcessorEditor)
};
