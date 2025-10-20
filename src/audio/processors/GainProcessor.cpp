#include "GainProcessor.h"

#if BROADCASTMIX_HAS_JUCE

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float kDecayFactor = 0.85F;
}

namespace broadcastmix::audio::processors {

GainProcessor::GainProcessor(float gainLinear,
                             juce::String name,
                             std::shared_ptr<MeterStore::MeterValue> meter,
                             juce::AudioChannelSet channelSet)
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", channelSet, channelSet != juce::AudioChannelSet::disabled())
                               .withOutput("Output", channelSet, channelSet != juce::AudioChannelSet::disabled()))
    , name_(std::move(name))
    , gainLinear_(gainLinear)
    , meter_(std::move(meter))
    , channelSet_(std::move(channelSet)) {}

const juce::String GainProcessor::getName() const {
    return name_;
}

void GainProcessor::prepareToPlay(double, int) {}

void GainProcessor::releaseResources() {}

void GainProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    midiMessages.clear();
    applyGain(buffer);
}

void GainProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) {
    midiMessages.clear();
    applyGain(buffer);
}

bool GainProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainInputChannelSet() == channelSet_
        && layouts.getMainOutputChannelSet() == channelSet_;
}

bool GainProcessor::acceptsMidi() const {
    return false;
}

bool GainProcessor::producesMidi() const {
    return false;
}

bool GainProcessor::isMidiEffect() const {
    return false;
}

double GainProcessor::getTailLengthSeconds() const {
    return 0.0;
}

bool GainProcessor::hasEditor() const {
    return false;
}

juce::AudioProcessorEditor* GainProcessor::createEditor() {
    return nullptr;
}

int GainProcessor::getNumPrograms() {
    return 1;
}

int GainProcessor::getCurrentProgram() {
    return 0;
}

void GainProcessor::setCurrentProgram(int) {}

const juce::String GainProcessor::getProgramName(int) {
    return "Default";
}

void GainProcessor::changeProgramName(int, const juce::String&) {}

void GainProcessor::getStateInformation(juce::MemoryBlock&) {}

void GainProcessor::setStateInformation(const void*, int) {}

template <typename SampleType>
void GainProcessor::applyGain(juce::AudioBuffer<SampleType>& buffer) {
    buffer.applyGain(static_cast<SampleType>(gainLinear_));
    const auto peaks = computePeaks(buffer);
    updateMeter(peaks);
}

template <typename SampleType>
std::array<float, 2> GainProcessor::computePeaks(const juce::AudioBuffer<SampleType>& buffer) const {
    std::array<float, 2> peaks { 0.0F, 0.0F };
    const auto channels = std::min(buffer.getNumChannels(), 2);
    const auto samples = buffer.getNumSamples();
    for (int channel = 0; channel < channels; ++channel) {
        SampleType peak { 0 };
        const auto* data = buffer.getReadPointer(channel);
        for (int sample = 0; sample < samples; ++sample) {
            peak = std::max(peak, static_cast<SampleType>(std::abs(data[sample])));
        }
        peaks[static_cast<std::size_t>(channel)] = std::clamp(static_cast<float>(peak), 0.0F, 1.0F);
    }
    return peaks;
}

void GainProcessor::updateMeter(const std::array<float, 2>& peaks) const {
    if (!meter_) {
        return;
    }

    for (std::size_t channel = 0; channel < peaks.size(); ++channel) {
        const auto current = meter_->channels[channel].load(std::memory_order_relaxed);
        const auto decayed = current * kDecayFactor;
        meter_->channels[channel].store(std::max(peaks[channel], decayed), std::memory_order_relaxed);
    }
}

template void GainProcessor::applyGain<float>(juce::AudioBuffer<float>&);
template void GainProcessor::applyGain<double>(juce::AudioBuffer<double>&);
template std::array<float, 2> GainProcessor::computePeaks<float>(const juce::AudioBuffer<float>&) const;
template std::array<float, 2> GainProcessor::computePeaks<double>(const juce::AudioBuffer<double>&) const;

} // namespace broadcastmix::audio::processors

#endif // BROADCASTMIX_HAS_JUCE
