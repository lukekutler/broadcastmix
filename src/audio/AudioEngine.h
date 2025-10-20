#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <array>

namespace broadcastmix::audio {

class GraphTopology;
class GraphNode;

struct AudioEngineSettings {
    std::uint32_t sampleRate { 48000 };
    std::uint32_t blockSize { 512 };
    std::uint32_t inputChannels { 32 };
    std::uint32_t outputChannels { 32 };
};

struct AudioEngineStatus {
    bool isRunning { false };
    double cpuLoad { 0.0 };
};

class AudioEngine {
public:
    explicit AudioEngine(AudioEngineSettings settings);
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) noexcept;
    AudioEngine& operator=(AudioEngine&&) noexcept;

    void start();
    void stop();

    [[nodiscard]] AudioEngineStatus status() const;
    [[nodiscard]] AudioEngineSettings settings() const;

    void setTopology(std::shared_ptr<GraphTopology> topology);
    [[nodiscard]] std::shared_ptr<const GraphTopology> topology() const;

    [[nodiscard]] std::array<float, 2> meterLevelsForNode(const std::string& nodeId) const;

    void processBlock();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace broadcastmix::audio
