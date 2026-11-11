#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr double pi = juce::MathConstants<double>::pi;

    const std::array<std::array<bool, 12>, 7> scaleIntervals {{
        // Major
        {{ true, false, true, false, true, true, false, true, false, true, false, true }},
        // Natural minor
        {{ true, false, true, true, false, true, false, true, true, false, true, false }},
        // Major pentatonic
        {{ true, false, true, false, true, false, false, true, false, true, false, false }},
        // Minor pentatonic
        {{ true, false, false, true, false, true, false, true, false, false, true, false }},
        // Harmonic minor
        {{ true, false, true, true, false, true, false, true, true, false, false, true }},
        // Dorian
        {{ true, false, true, true, false, true, false, true, false, true, true, false }},
        // Chromatic
        {{ true, true, true, true, true, true, true, true, true, true, true, true }}
    }};

    const juce::StringArray noteNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    juce::StringArray scaleNames {
        "Major", "Natural Minor", "Major Pentatonic", "Minor Pentatonic",
        "Harmonic Minor", "Dorian", "Chromatic"
    };
}

AntihumanScaleAudioProcessor::AntihumanScaleAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (auto& point : spectrumDisplay)
        point.store(0.0f, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout
AntihumanScaleAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<APC>("root", "Root", noteNames, 0));
    layout.add(std::make_unique<APC>("scale", "Scale", scaleNames, 0));
    layout.add(std::make_unique<APF>("amount", "Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    layout.add(std::make_unique<APF>("mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    layout.add(std::make_unique<APF>("colour", "Colour",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 15.0f));
    layout.add(std::make_unique<APF>("morph", "Morph",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<APF>("gate", "Gate",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<APF>("transpose", "Pitch",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<APB>("midi", "MIDI Root", false));

    return layout;
}

void AntihumanScaleAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 1000.0 ? sampleRate : 44100.0;
    samplesProcessed = 0;
    activeChannels = juce::jmax(1, juce::jmin(2, getTotalNumOutputChannels()));

    window.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
    {
        const auto phase = static_cast<double>(i) / static_cast<double>(fftSize - 1);
        window[static_cast<size_t>(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * pi * phase));
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        inputRing[static_cast<size_t>(channel)].assign(fftSize, 0.0f);
        outputRing[static_cast<size_t>(channel)].assign(ringSize, 0.0f);
        fftWork[static_cast<size_t>(channel)].assign(2 * fftSize, 0.0f);
        fftProcessed[static_cast<size_t>(channel)].assign(2 * fftSize, 0.0f);
        binWeights[static_cast<size_t>(channel)].assign(fftSize / 2 + 1, 0.0f);
        dryMagnitudes[static_cast<size_t>(channel)].assign(fftSize / 2 + 1, 0.0f);
    }

    setLatencySamples(fftSize);
}

void AntihumanScaleAudioProcessor::releaseResources()
{
    for (auto& channel : inputRing)
        std::fill(channel.begin(), channel.end(), 0.0f);
    for (auto& channel : outputRing)
        std::fill(channel.begin(), channel.end(), 0.0f);
}

bool AntihumanScaleAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainInput != mainOutput)
        return false;
    return mainInput == juce::AudioChannelSet::mono()
        || mainInput == juce::AudioChannelSet::stereo();
}

float AntihumanScaleAudioProcessor::readParameter(const char* parameterID) const noexcept
{
    if (auto* value = parameters.getRawParameterValue(parameterID))
        return value->load(std::memory_order_relaxed);
    return 0.0f;
}

bool AntihumanScaleAudioProcessor::noteIsInScale(int midiNote, int rootNote, int scaleIndex) noexcept
{
    const auto safeScale = juce::jlimit(0, static_cast<int>(scaleIntervals.size()) - 1, scaleIndex);
    auto pitchClass = (midiNote - rootNote) % 12;
    if (pitchClass < 0)
        pitchClass += 12;
    return scaleIntervals[static_cast<size_t>(safeScale)][static_cast<size_t>(pitchClass)];
}

double AntihumanScaleAudioProcessor::nearestScaleNote(double midiNote, int rootNote, int scaleIndex) noexcept
{
    const int centre = static_cast<int>(std::floor(midiNote + 0.5));
    double bestNote = static_cast<double>(centre);
    double bestDistance = 1.0e9;

    for (int candidate = centre - 12; candidate <= centre + 12; ++candidate)
    {
        if (!noteIsInScale(candidate, rootNote, scaleIndex))
            continue;

        const auto distance = std::abs(static_cast<double>(candidate) - midiNote);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNote = static_cast<double>(candidate);
        }
    }

    return bestNote;
}

void AntihumanScaleAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear(channel, 0, numSamples);

    const bool midiEnabled = readParameter("midi") >= 0.5f;
    if (midiEnabled)
    {
        for (const auto metadata : midiMessages)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn())
                midiRootNote.store(message.getNoteNumber() % 12, std::memory_order_relaxed);
            else if (message.isAllNotesOff() || message.isAllSoundOff())
                midiRootNote.store(-1, std::memory_order_relaxed);
        }
    }
    else
    {
        midiRootNote.store(-1, std::memory_order_relaxed);
    }

    const int selectedRoot = juce::jlimit(0, 11, static_cast<int>(std::round(readParameter("root"))));
    const int root = (midiEnabled && midiRootNote.load(std::memory_order_relaxed) >= 0)
        ? midiRootNote.load(std::memory_order_relaxed) : selectedRoot;
    const int scale = juce::jlimit(0, 6, static_cast<int>(std::round(readParameter("scale"))));
    const float amount = readParameter("amount") * 0.01f;
    const float mix = readParameter("mix") * 0.01f;
    const float colour = readParameter("colour") * 0.01f;
    const float morph = readParameter("morph") * 0.01f;
    const float gate = readParameter("gate") * 0.01f;
    const float transpose = readParameter("transpose");

    auto* left = buffer.getWritePointer(0);
    auto* right = numInputChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto absoluteSample = samplesProcessed;
        const auto inputIndex = static_cast<size_t>(absoluteSample % static_cast<std::uint64_t>(fftSize));
        const auto outputIndex = static_cast<size_t>(absoluteSample % static_cast<std::uint64_t>(ringSize));

        float drySamples[2] { left[sample], right != nullptr ? right[sample] : left[sample] };
        float delayedDrySamples[2] { 0.0f, 0.0f };
        float wetSamples[2] { 0.0f, 0.0f };

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            auto& in = inputRing[static_cast<size_t>(channel)];
            auto& out = outputRing[static_cast<size_t>(channel)];
            // Read the sample from one FFT frame ago before overwriting this ring slot.
            delayedDrySamples[channel] = in[inputIndex];
            in[inputIndex] = drySamples[channel];
            wetSamples[channel] = out[outputIndex];
            out[outputIndex] = 0.0f;
        }

        const float outputLeft = delayedDrySamples[0] * (1.0f - mix) + wetSamples[0] * mix;
        left[sample] = outputLeft;
        if (right != nullptr)
        {
            const float outputRight = delayedDrySamples[1] * (1.0f - mix) + wetSamples[1] * mix;
            right[sample] = outputRight;
        }

        ++samplesProcessed;
        if (samplesProcessed >= static_cast<std::uint64_t>(fftSize)
            && (samplesProcessed % static_cast<std::uint64_t>(hopSize)) == 0)
        {
            processSpectralFrame(root, scale, amount, colour, morph, gate, transpose);
        }
    }
}

void AntihumanScaleAudioProcessor::processSpectralFrame(int rootNote, int scaleIndex,
                                                        float amount, float colour,
                                                        float morph, float gate,
                                                        float transposeSemitones)
{
    const auto frameStart = samplesProcessed - static_cast<std::uint64_t>(fftSize);
    const auto positiveBins = fftSize / 2 + 1;
    const auto firstOutputSample = frameStart + static_cast<std::uint64_t>(fftSize);
    const auto gateDb = -90.0f + gate * 55.0f;
    const auto gateThreshold = std::pow(10.0f, gateDb / 20.0f);

    for (int channel = 0; channel < activeChannels; ++channel)
    {
        auto& input = inputRing[static_cast<size_t>(channel)];
        auto& output = outputRing[static_cast<size_t>(channel)];
        auto& work = fftWork[static_cast<size_t>(channel)];
        auto& processed = fftProcessed[static_cast<size_t>(channel)];
        auto& weights = binWeights[static_cast<size_t>(channel)];
        auto& dryMag = dryMagnitudes[static_cast<size_t>(channel)];

        std::fill(work.begin(), work.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
        {
            const auto sourceIndex = static_cast<size_t>((frameStart + static_cast<std::uint64_t>(i))
                % static_cast<std::uint64_t>(fftSize));
            work[static_cast<size_t>(2 * i)] = input[sourceIndex] * window[static_cast<size_t>(i)];
        }

        // JUCE 9 expects std::complex<float> buffers, not interleaved float*.
        // Convert the interleaved work buffer to complex samples, run the FFT,
        // then copy the spectrum back into the existing interleaved layout.
        for (int i = 0; i < fftSize; ++i)
            fftInput[static_cast<size_t>(i)] = {
                work[static_cast<size_t>(2 * i)],
                work[static_cast<size_t>(2 * i + 1)]
            };

        fft.perform(fftInput.data(), fftOutput.data(), false);

        for (int i = 0; i < fftSize; ++i)
        {
            work[static_cast<size_t>(2 * i)] = fftOutput[static_cast<size_t>(i)].real();
            work[static_cast<size_t>(2 * i + 1)] = fftOutput[static_cast<size_t>(i)].imag();
        }

        float framePeak = 0.0f;
        for (int bin = 0; bin < positiveBins; ++bin)
        {
            const auto re = work[static_cast<size_t>(2 * bin)];
            const auto im = work[static_cast<size_t>(2 * bin + 1)];
            const auto magnitude = std::sqrt(re * re + im * im);
            dryMag[static_cast<size_t>(bin)] = magnitude;
            framePeak = juce::jmax(framePeak, magnitude);
        }

        std::fill(processed.begin(), processed.end(), 0.0f);
        std::fill(weights.begin(), weights.end(), 0.0f);

        // DC and Nyquist are kept in place.
        processed[0] = work[0];
        processed[1] = work[1];
        processed[static_cast<size_t>(fftSize)] = work[static_cast<size_t>(fftSize)];
        processed[static_cast<size_t>(fftSize + 1)] = work[static_cast<size_t>(fftSize + 1)];
        weights[0] = 1.0f;
        weights[static_cast<size_t>(fftSize / 2)] = 1.0f;

        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const auto re = work[static_cast<size_t>(2 * bin)];
            const auto im = work[static_cast<size_t>(2 * bin + 1)];
            const auto magnitude = dryMag[static_cast<size_t>(bin)];

            if (gate > 0.001f && framePeak > 1.0e-8f
                && magnitude < framePeak * gateThreshold)
                continue;

            const auto frequency = static_cast<double>(bin) * currentSampleRate
                / static_cast<double>(fftSize);
            double destinationFrequency = frequency;

            if (frequency >= 40.0 && frequency <= 9000.0
                && (amount > 0.0001f || std::abs(transposeSemitones) > 0.0001f))
            {
                const auto midiNote = 69.0 + 12.0 * std::log2(frequency / 440.0)
                    + static_cast<double>(transposeSemitones);
                const auto snappedNote = nearestScaleNote(midiNote, rootNote, scaleIndex);
                const auto tunedNote = midiNote + static_cast<double>(amount) * (snappedNote - midiNote);
                destinationFrequency = 440.0 * std::pow(2.0, (tunedNote - 69.0) / 12.0);
            }

            const auto destinationBin = juce::jlimit(1.0, static_cast<double>(fftSize / 2 - 1),
                destinationFrequency * static_cast<double>(fftSize) / currentSampleRate);
            const int lowerBin = static_cast<int>(std::floor(destinationBin));
            const int upperBin = juce::jmin(fftSize / 2 - 1, lowerBin + 1);
            const float upperWeight = static_cast<float>(destinationBin - static_cast<double>(lowerBin));
            const float lowerWeight = 1.0f - upperWeight;

            const auto addToBin = [&](int targetBin, float weight)
            {
                if (weight <= 0.0f)
                    return;
                const auto target = static_cast<size_t>(2 * targetBin);
                processed[target] += re * weight;
                processed[target + 1] += im * weight;
                weights[static_cast<size_t>(targetBin)] += weight;
            };

            addToBin(lowerBin, lowerWeight);
            addToBin(upperBin, upperWeight);
        }

        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            auto& re = processed[static_cast<size_t>(2 * bin)];
            auto& im = processed[static_cast<size_t>(2 * bin + 1)];
            const auto weight = weights[static_cast<size_t>(bin)];
            if (weight > 1.0f)
            {
                const auto normalizer = 1.0f / std::sqrt(weight);
                re *= normalizer;
                im *= normalizer;
            }

            auto magnitude = std::sqrt(re * re + im * im);
            const auto dryMagnitude = dryMag[static_cast<size_t>(bin)];

            if (morph > 0.0001f)
            {
                const auto blendedMagnitude = magnitude * (1.0f - morph) + dryMagnitude * morph;
                if (magnitude > 1.0e-9f)
                {
                    const auto scale = blendedMagnitude / magnitude;
                    re *= scale;
                    im *= scale;
                }
                else
                {
                    re = work[static_cast<size_t>(2 * bin)];
                    im = work[static_cast<size_t>(2 * bin + 1)];
                    magnitude = dryMagnitude;
                }
                magnitude = blendedMagnitude;
            }

            // Colour adds a restrained emphasis around frequencies that sit close to scale notes.
            if (colour > 0.0001f && magnitude > 1.0e-9f)
            {
                const auto frequency = static_cast<double>(bin) * currentSampleRate
                    / static_cast<double>(fftSize);
                if (frequency >= 40.0)
                {
                    const auto midi = 69.0 + 12.0 * std::log2(frequency / 440.0);
                    const auto nearest = nearestScaleNote(midi, rootNote, scaleIndex);
                    const auto cents = std::abs(midi - nearest) * 100.0;
                    const auto resonance = std::exp(-0.5 * (cents / 32.0) * (cents / 32.0));
                    const auto gain = static_cast<float>(1.0 + 0.55 * colour * resonance);
                    re *= gain;
                    im *= gain;
                }
            }

            // Rebuild the negative-frequency half as the complex conjugate.
            const int mirrorBin = fftSize - bin;
            processed[static_cast<size_t>(2 * mirrorBin)] = re;
            processed[static_cast<size_t>(2 * mirrorBin + 1)] = -im;
        }

        // Convert the processed interleaved spectrum to JUCE 9 complex buffers.
        for (int i = 0; i < fftSize; ++i)
            fftInput[static_cast<size_t>(i)] = {
                processed[static_cast<size_t>(2 * i)],
                processed[static_cast<size_t>(2 * i + 1)]
            };

        fft.perform(fftInput.data(), fftOutput.data(), true);

        for (int i = 0; i < fftSize; ++i)
        {
            processed[static_cast<size_t>(2 * i)] = fftOutput[static_cast<size_t>(i)].real();
            processed[static_cast<size_t>(2 * i + 1)] = fftOutput[static_cast<size_t>(i)].imag();
        }

        for (int i = 0; i < fftSize; ++i)
        {
            const auto outputAbsolute = firstOutputSample + static_cast<std::uint64_t>(i);
            const auto outputIndex = static_cast<size_t>(outputAbsolute % static_cast<std::uint64_t>(ringSize));
            const auto sample = processed[static_cast<size_t>(2 * i)]
                / static_cast<float>(fftSize) * window[static_cast<size_t>(i)] * (2.0f / 3.0f);
            output[outputIndex] += sample;
        }

        // Publish a compact logarithmic spectrum for the editor. Only channel 0 is displayed.
        if (channel == 0)
        {
            for (int point = 0; point < spectrumPoints; ++point)
            {
                const auto lowFrequency = 100.0 * std::pow(6000.0 / 100.0,
                    static_cast<double>(point) / static_cast<double>(spectrumPoints));
                const auto highFrequency = 100.0 * std::pow(6000.0 / 100.0,
                    static_cast<double>(point + 1) / static_cast<double>(spectrumPoints));
                const int lowBin = juce::jlimit(1, fftSize / 2 - 1,
                    static_cast<int>(lowFrequency * fftSize / currentSampleRate));
                const int highBin = juce::jlimit(lowBin + 1, fftSize / 2,
                    static_cast<int>(highFrequency * fftSize / currentSampleRate));
                float bandPeak = 0.0f;
                for (int bin = lowBin; bin < highBin; ++bin)
                    bandPeak = juce::jmax(bandPeak, dryMag[static_cast<size_t>(bin)]);
                const auto normalized = framePeak > 1.0e-8f
                    ? juce::jlimit(0.0f, 1.0f, bandPeak / framePeak) : 0.0f;
                spectrumDisplay[static_cast<size_t>(point)].store(normalized, std::memory_order_relaxed);
            }
        }
    }
}

float AntihumanScaleAudioProcessor::getSpectrumPoint(int index) const noexcept
{
    if (index < 0 || index >= spectrumPoints)
        return 0.0f;
    return spectrumDisplay[static_cast<size_t>(index)].load(std::memory_order_relaxed);
}

juce::AudioProcessorEditor* AntihumanScaleAudioProcessor::createEditor()
{
    return new AntihumanScaleAudioProcessorEditor(*this);
}

void AntihumanScaleAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AntihumanScaleAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AntihumanScaleAudioProcessor();
}
