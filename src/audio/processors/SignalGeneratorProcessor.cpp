#include "SignalGeneratorProcessor.h"

#if BROADCASTMIX_HAS_JUCE

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kTargetFrequencyHz = 1000.0;
constexpr float kAmplitude = 1.0F; // 0 dBFS
constexpr float kDecayFactor = 0.85F;
} // namespace

namespace broadcastmix::audio::processors {

SignalGeneratorProcessor::SignalGeneratorProcessor(std::shared_ptr<MeterStore::MeterValue> meter,
                                                   juce::AudioChannelSet channelSet)
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", channelSet, channelSet != juce::AudioChannelSet::disabled())
                               .withOutput("Output", channelSet, true))
    , meter_(std::move(meter))
    , channelSet_(std::move(channelSet)) {}

const juce::String SignalGeneratorProcessor::getName() const {
    return "Signal Generator";
}

void SignalGeneratorProcessor::prepareToPlay(double sampleRate, int) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    phase_ = 0.0;
    phaseIncrement_ = kTwoPi * kTargetFrequencyHz / sampleRate_;
}

void SignalGeneratorProcessor::releaseResources() {}

void SignalGeneratorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    midiMessages.clear();
    process(buffer);
}

void SignalGeneratorProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) {
    midiMessages.clear();
    process(buffer);
}

bool SignalGeneratorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto outputSet = layouts.getMainOutputChannelSet();
    if (outputSet != channelSet_) {
        return false;
    }

    const auto inputSet = layouts.getMainInputChannelSet();
    return inputSet == juce::AudioChannelSet::disabled() || inputSet == outputSet;
}

bool SignalGeneratorProcessor::acceptsMidi() const {
    return false;
}

bool SignalGeneratorProcessor::producesMidi() const {
    return false;
}

bool SignalGeneratorProcessor::isMidiEffect() const {
    return false;
}

double SignalGeneratorProcessor::getTailLengthSeconds() const {
    return 0.0;
}

bool SignalGeneratorProcessor::hasEditor() const {
    return false;
}

juce::AudioProcessorEditor* SignalGeneratorProcessor::createEditor() {
    return nullptr;
}

int SignalGeneratorProcessor::getNumPrograms() {
    return 1;
}

int SignalGeneratorProcessor::getCurrentProgram() {
    return 0;
}

void SignalGeneratorProcessor::setCurrentProgram(int) {}

const juce::String SignalGeneratorProcessor::getProgramName(int) {
    return "Default";
}

void SignalGeneratorProcessor::changeProgramName(int, const juce::String&) {}

void SignalGeneratorProcessor::getStateInformation(juce::MemoryBlock&) {}

void SignalGeneratorProcessor::setStateInformation(const void*, int) {}

template <typename SampleType>
void SignalGeneratorProcessor::process(juce::AudioBuffer<SampleType>& buffer) {
    auto output = getBusBuffer(buffer, false, 0);
    const auto numSamples = output.getNumSamples();
    const auto numOutputChannels = output.getNumChannels();

    juce::AudioBuffer<SampleType> input;
    bool hasInput = false;
    if (getBusCount(true) > 0) {
        const auto layout = getBusesLayout();
        if (layout.getMainInputChannelSet() != juce::AudioChannelSet::disabled()) {
            input = getBusBuffer(buffer, true, 0);
            hasInput = input.getNumChannels() > 0;
        }
    }

    if (hasInput) {
        const bool sharesBuffer =
            input.getNumChannels() > 0 && output.getNumChannels() > 0 &&
            input.getWritePointer(0) == output.getWritePointer(0);

        if (!sharesBuffer) {
            const auto copyChannels = std::min(input.getNumChannels(), numOutputChannels);
            for (int channel = 0; channel < copyChannels; ++channel) {
                output.copyFrom(channel, 0, input, channel, 0, numSamples);
            }
            for (int channel = copyChannels; channel < numOutputChannels; ++channel) {
                output.clear(channel, 0, numSamples);
            }
        }
    } else {
        output.clear();
    }

    addGeneratedSamples(output);

    std::array<float, 2> peaks { 0.0F, 0.0F };
    const auto limitedChannels = std::min(output.getNumChannels(), 2);
    for (int channel = 0; channel < limitedChannels; ++channel) {
        SampleType peak { 0 };
        const auto* channelData = output.getReadPointer(channel);
        for (int sample = 0; sample < numSamples; ++sample) {
            peak = std::max(peak, static_cast<SampleType>(std::abs(channelData[sample])));
        }
        peaks[static_cast<std::size_t>(channel)] = std::clamp(static_cast<float>(peak), 0.0F, 1.0F);
    }

    updateMeter(peaks);
}

template <typename SampleType>
void SignalGeneratorProcessor::addGeneratedSamples(juce::AudioBuffer<SampleType>& buffer) {
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample) {
        const auto value = static_cast<SampleType>(std::sin(phase_) * kAmplitude);
        for (int channel = 0; channel < numChannels; ++channel) {
            buffer.addSample(channel, sample, value);
        }

        phase_ += phaseIncrement_;
        if (phase_ >= kTwoPi) {
            phase_ -= kTwoPi;
        }
    }
}

void SignalGeneratorProcessor::updateMeter(const std::array<float, 2>& peaks) {
    if (!meter_) {
        return;
    }

    for (std::size_t channel = 0; channel < peaks.size(); ++channel) {
        const auto current = meter_->channels[channel].load(std::memory_order_relaxed);
        const auto decayed = current * kDecayFactor;
        const auto next = std::max(peaks[channel], decayed);
        meter_->channels[channel].store(next, std::memory_order_relaxed);
    }
}

template void SignalGeneratorProcessor::process<float>(juce::AudioBuffer<float>&);
template void SignalGeneratorProcessor::process<double>(juce::AudioBuffer<double>&);
template void SignalGeneratorProcessor::addGeneratedSamples<float>(juce::AudioBuffer<float>&);
template void SignalGeneratorProcessor::addGeneratedSamples<double>(juce::AudioBuffer<double>&);

} // namespace broadcastmix::audio::processors

#endif // BROADCASTMIX_HAS_JUCE
