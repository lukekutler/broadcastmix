#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace broadcastmix::audio {

enum class GraphNodeType {
    Input,
    Channel,
    GroupBus,
    BroadcastBus,
    MixBus,
    Utility,
    Plugin,
    SignalGenerator,
    Output
};

struct GraphConnection {
    std::string fromNodeId;
    std::uint32_t fromChannel { 0 };
    std::string toNodeId;
    std::uint32_t toChannel { 0 };
};

class GraphNode {
public:
    GraphNode(std::string id, GraphNodeType type);

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] GraphNodeType type() const noexcept;

    void setLabel(std::string_view label);
    [[nodiscard]] const std::string& label() const noexcept;

    void addInputChannel();
    void addOutputChannel();
    void setInputChannelCount(std::uint32_t count);
    void setOutputChannelCount(std::uint32_t count);
    [[nodiscard]] std::uint32_t inputChannelCount() const noexcept;
    [[nodiscard]] std::uint32_t outputChannelCount() const noexcept;
    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;

private:
    std::string id_;
    GraphNodeType type_;
    std::string label_;
    std::uint32_t inputChannels_ { 0 };
    std::uint32_t outputChannels_ { 0 };
    bool enabled_ { true };
};

using GraphNodeList = std::vector<GraphNode>;
using GraphConnectionList = std::vector<GraphConnection>;

} // namespace broadcastmix::audio
