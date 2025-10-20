#pragma once

#if BROADCASTMIX_HAS_JUCE

#include <memory>

#include "../MeterStore.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace broadcastmix::audio::processors {

class PassThroughProcessor : public juce::AudioProcessor {
public:
    PassThroughProcessor(juce::String name,
                         std::shared_ptr<MeterStore::MeterValue> meter,
                         juce::AudioChannelSet channelSet = juce::AudioChannelSet::stereo());

    const juce::String getName() const override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    template <typename SampleType>
    void updateMeterFromBuffer(const juce::AudioBuffer<SampleType>& buffer);

    juce::String name_;
    std::shared_ptr<MeterStore::MeterValue> meter_;
};

} // namespace broadcastmix::audio::processors

#endif // BROADCASTMIX_HAS_JUCE
