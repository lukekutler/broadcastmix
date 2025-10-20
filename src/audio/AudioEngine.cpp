#include "AudioEngine.h"

#include "GraphTopology.h"
#include "MeterStore.h"
#include "../core/Logging.h"

#include <algorithm>
#include <utility>

#if BROADCASTMIX_HAS_JUCE
#include "JuceGraphBuilder.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#endif

namespace broadcastmix::audio {

struct AudioEngine::Impl {
    explicit Impl(AudioEngineSettings cfg)
        : config(std::move(cfg)) {
#if BROADCASTMIX_HAS_JUCE
        deviceManager = std::make_unique<juce::AudioDeviceManager>();

        processorGraph = std::make_unique<juce::AudioProcessorGraph>();
        processorPlayer = std::make_unique<juce::AudioProcessorPlayer>();
        processorPlayer->setProcessor(processorGraph.get());
        meterStore = std::make_shared<MeterStore>();
        builder = std::make_unique<JuceGraphBuilder>(*processorGraph, meterStore);
#endif
    }

    ~Impl() {
#if BROADCASTMIX_HAS_JUCE
        if (processorPlayer) {
            processorPlayer->setProcessor(nullptr);
        }
#endif
    }

    AudioEngineSettings config;
    AudioEngineStatus status {};
    std::shared_ptr<GraphTopology> topology;
#if BROADCASTMIX_HAS_JUCE
    std::unique_ptr<juce::AudioDeviceManager> deviceManager;
    std::unique_ptr<juce::AudioProcessorGraph> processorGraph;
    std::unique_ptr<juce::AudioProcessorPlayer> processorPlayer;
    std::unique_ptr<JuceGraphBuilder> builder;
    std::shared_ptr<MeterStore> meterStore;
    bool deviceInitialised { false };

    void ensureDeviceInitialised() {
        if (deviceInitialised || deviceManager == nullptr) {
            return;
        }

        const auto requestedInputs = static_cast<int>(config.inputChannels > 0 ? config.inputChannels : 2);
        const auto requestedOutputs = static_cast<int>(config.outputChannels > 0 ? config.outputChannels : 2);
        const auto result = deviceManager->initialiseWithDefaultDevices(requestedInputs, requestedOutputs);
        if (result.isNotEmpty()) {
            core::log(core::LogCategory::Audio, "Audio device init warning: {}", result.toStdString());
        }

        deviceInitialised = true;
    }
#endif
};

AudioEngine::AudioEngine(AudioEngineSettings settings)
    : impl_(std::make_unique<Impl>(std::move(settings))) {}

AudioEngine::~AudioEngine() = default;

AudioEngine::AudioEngine(AudioEngine&&) noexcept = default;
AudioEngine& AudioEngine::operator=(AudioEngine&&) noexcept = default;

void AudioEngine::start() {
    if (impl_->status.isRunning) {
        return;
    }

#if BROADCASTMIX_HAS_JUCE
    if (!impl_->topology) {
        setTopology(std::make_shared<GraphTopology>(GraphTopology::createDefaultBroadcastLayout()));
    }

    if (impl_->deviceManager && impl_->processorPlayer) {
        impl_->ensureDeviceInitialised();
        impl_->deviceManager->addAudioCallback(impl_->processorPlayer.get());
    }
#endif

    impl_->status.isRunning = true;
    core::log(core::LogCategory::Audio, "Audio engine started");
}

void AudioEngine::stop() {
    if (!impl_->status.isRunning) {
        return;
    }

    impl_->status.isRunning = false;
#if BROADCASTMIX_HAS_JUCE
    if (impl_->deviceManager && impl_->processorPlayer) {
        impl_->deviceManager->removeAudioCallback(impl_->processorPlayer.get());
    }
#endif
    core::log(core::LogCategory::Audio, "Audio engine stopped");
}

AudioEngineStatus AudioEngine::status() const {
    return impl_->status;
}

AudioEngineSettings AudioEngine::settings() const {
    return impl_->config;
}

void AudioEngine::setTopology(std::shared_ptr<GraphTopology> topology) {
    if (!topology) {
        topology = std::make_shared<GraphTopology>(GraphTopology::createDefaultBroadcastLayout());
    }

    impl_->topology = std::move(topology);
#if BROADCASTMIX_HAS_JUCE
    if (impl_->builder && impl_->topology) {
        if (impl_->meterStore) {
            impl_->meterStore->syncWithTopology(*impl_->topology);
        }
        impl_->builder->rebuildFromTopology(*impl_->topology);
    }
#endif
    core::log(core::LogCategory::Audio, "Topology assigned to audio engine");
}

std::shared_ptr<const GraphTopology> AudioEngine::topology() const {
    return impl_->topology;
}

std::array<float, 2> AudioEngine::meterLevelsForNode(const std::string& nodeId) const {
#if BROADCASTMIX_HAS_JUCE
    if (impl_->meterStore) {
        return impl_->meterStore->levelsFor(nodeId);
    }
#else
    (void) nodeId;
#endif
    return { 0.0F, 0.0F };
}

void AudioEngine::processBlock() {
    // Offline processing not yet implemented; runtime uses JUCE audio callbacks.
}

} // namespace broadcastmix::audio
