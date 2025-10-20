#include "PassThroughProcessor.h"

#if BROADCASTMIX_HAS_JUCE

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float kDecayFactor = 0.85F;
}

namespace broadcastmix::audio::processors {

PassThroughProcessor::PassThroughProcessor(juce::String name,
                                           std::shared_ptr<MeterStore::MeterValue> meter,
                                           juce::AudioChannelSet channelSet)
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", channelSet, channelSet != juce::AudioChannelSet::disabled())
                               .withOutput("Output", channelSet, channelSet != juce::AudioChannelSet::disabled()))
    , name_(std::move(name))
    , meter_(std::move(meter)) {}

const juce::String PassThroughProcessor::getName() const {
    return name_;
}

void PassThroughProcessor::prepareToPlay(double, int) {}

void PassThroughProcessor::releaseResources() {}

void PassThroughProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    midiMessages.clear();
    updateMeterFromBuffer(buffer);
}

void PassThroughProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) {
    midiMessages.clear();
    updateMeterFromBuffer(buffer);
}

bool PassThroughProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

bool PassThroughProcessor::acceptsMidi() const {
    return false;
}

bool PassThroughProcessor::producesMidi() const {
    return false;
}

bool PassThroughProcessor::isMidiEffect() const {
    return false;
}

double PassThroughProcessor::getTailLengthSeconds() const {
    return 0.0;
}

bool PassThroughProcessor::hasEditor() const {
    return false;
}

juce::AudioProcessorEditor* PassThroughProcessor::createEditor() {
    return nullptr;
}

int PassThroughProcessor::getNumPrograms() {
    return 1;
}

int PassThroughProcessor::getCurrentProgram() {
    return 0;
}

void PassThroughProcessor::setCurrentProgram(int) {}

const juce::String PassThroughProcessor::getProgramName(int) {
    return "Default";
}

void PassThroughProcessor::changeProgramName(int, const juce::String&) {}

void PassThroughProcessor::getStateInformation(juce::MemoryBlock&) {}

void PassThroughProcessor::setStateInformation(const void*, int) {}

template <typename SampleType>
void PassThroughProcessor::updateMeterFromBuffer(const juce::AudioBuffer<SampleType>& buffer) {
    if (!meter_) {
        return;
    }

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

    for (std::size_t channel = 0; channel < peaks.size(); ++channel) {
        const auto current = meter_->channels[channel].load(std::memory_order_relaxed);
        const auto decayed = current * kDecayFactor;
        meter_->channels[channel].store(std::max(peaks[channel], decayed), std::memory_order_relaxed);
    }
}

template void PassThroughProcessor::updateMeterFromBuffer<float>(const juce::AudioBuffer<float>&);
template void PassThroughProcessor::updateMeterFromBuffer<double>(const juce::AudioBuffer<double>&);

} // namespace broadcastmix::audio::processors

#endif // BROADCASTMIX_HAS_JUCE
