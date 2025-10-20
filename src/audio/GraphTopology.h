#pragma once

#include "GraphNode.h"

#include <optional>
#include <string_view>
#include <unordered_map>

namespace broadcastmix::audio {

class GraphTopology {
public:
    GraphTopology();

    GraphNode& addNode(GraphNode node);
    void removeNode(const std::string& id);

    void connect(GraphConnection connection);
    void disconnect(const std::string& fromId, const std::string& toId);

    [[nodiscard]] const GraphNodeList& nodes() const noexcept;
    [[nodiscard]] const GraphConnectionList& connections() const noexcept;

    [[nodiscard]] std::optional<GraphNode> findNode(const std::string& id) const;
    bool setNodeChannelCounts(const std::string& id, std::uint32_t inputChannels, std::uint32_t outputChannels);
    void setNodeLabel(const std::string& id, std::string_view label);
    void setNodeEnabled(const std::string& id, bool enabled);
    [[nodiscard]] bool isNodeEnabled(const std::string& id) const;
    [[nodiscard]] bool connectionExists(const std::string& fromId,
                                        const std::string& toId,
                                        std::uint32_t fromChannel,
                                        std::uint32_t toChannel) const;

    static GraphTopology createDefaultBroadcastLayout();
    static GraphTopology createGroupMicroLayout(std::string_view groupId);
    static GraphTopology createChannelMicroLayout(std::string_view channelId);
    static GraphTopology createOutputMicroLayout(std::string_view outputId);

private:
    GraphNodeList nodes_;
    GraphConnectionList connections_;
    std::unordered_map<std::string, std::size_t> nodeIndex_;
    void pruneConnectionsForNode(const std::string& id, std::uint32_t inputChannels, std::uint32_t outputChannels);
};

} // namespace broadcastmix::audio
