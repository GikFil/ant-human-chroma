#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class AntihumanScaleAudioProcessor : public juce::AudioProcessor
{
public:
    AntihumanScaleAudioProcessor();
    ~AntihumanScaleAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

    float getSpectrumPoint(int index) const noexcept;
    int getMidiRoot() const noexcept { return midiRootNote.load(std::memory_order_relaxed); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int ringSize = fftSize * 8;
    static constexpr int spectrumPoints = 64;

    juce::dsp::FFT fft { fftOrder };
    std::array<std::vector<float>, 2> inputRing;
    std::array<std::vector<float>, 2> outputRing;
    std::array<std::vector<float>, 2> fftWork;
    std::array<std::vector<float>, 2> fftProcessed;
    std::array<std::vector<float>, 2> binWeights;
    std::array<std::vector<float>, 2> dryMagnitudes;
    std::vector<float> window;
    std::array<std::atomic<float>, spectrumPoints> spectrumDisplay;

    double currentSampleRate = 44100.0;
    std::uint64_t samplesProcessed = 0;
    std::atomic<int> midiRootNote { -1 };
    int activeChannels = 2;

    void processSpectralFrame(int rootNote, int scaleIndex, float amount,
                              float colour, float morph, float gate,
                              float transposeSemitones);
    static double nearestScaleNote(double midiNote, int rootNote, int scaleIndex) noexcept;
    static bool noteIsInScale(int midiNote, int rootNote, int scaleIndex) noexcept;
    float readParameter(const char* parameterID) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AntihumanScaleAudioProcessor)
};
