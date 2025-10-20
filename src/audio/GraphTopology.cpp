#include "GraphTopology.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace broadcastmix::audio {

GraphTopology::GraphTopology() = default;

GraphNode& GraphTopology::addNode(GraphNode node) {
    const auto id = node.id();
    nodes_.push_back(std::move(node));
    nodeIndex_[id] = nodes_.size() - 1;
    return nodes_.back();
}

void GraphTopology::removeNode(const std::string& id) {
    const auto it = nodeIndex_.find(id);
    if (it == nodeIndex_.end()) {
        return;
    }

    const auto idx = it->second;
    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(idx));
    nodeIndex_.erase(it);

    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                       [&](const auto& connection) {
                           return connection.fromNodeId == id || connection.toNodeId == id;
                       }),
        connections_.end());

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        nodeIndex_[nodes_[i].id()] = i;
    }
}

void GraphTopology::connect(GraphConnection connection) {
    if (connectionExists(connection.fromNodeId, connection.toNodeId, connection.fromChannel, connection.toChannel)) {
        return;
    }
    connections_.push_back(std::move(connection));
}

void GraphTopology::disconnect(const std::string& fromId, const std::string& toId) {
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                       [&](const auto& connection) {
                           return connection.fromNodeId == fromId && connection.toNodeId == toId;
                       }),
        connections_.end());
}

const GraphNodeList& GraphTopology::nodes() const noexcept {
    return nodes_;
}

const GraphConnectionList& GraphTopology::connections() const noexcept {
    return connections_;
}

std::optional<GraphNode> GraphTopology::findNode(const std::string& id) const {
    const auto it = nodeIndex_.find(id);
    if (it == nodeIndex_.end()) {
        return std::nullopt;
    }

    return nodes_[it->second];
}

void GraphTopology::setNodeLabel(const std::string& id, std::string_view label) {
    const auto it = nodeIndex_.find(id);
    if (it == nodeIndex_.end()) {
        return;
    }
    nodes_[it->second].setLabel(label);
}

bool GraphTopology::setNodeChannelCounts(const std::string& id, std::uint32_t inputChannels, std::uint32_t outputChannels) {
    const auto it = nodeIndex_.find(id);
    if (it == nodeIndex_.end()) {
        return false;
    }

    nodes_[it->second].setInputChannelCount(inputChannels);
    nodes_[it->second].setOutputChannelCount(outputChannels);
    pruneConnectionsForNode(id, inputChannels, outputChannels);
    return true;
}

void GraphTopology::setNodeEnabled(const std::string& id, bool enabled) {
    const auto it = nodeIndex_.find(id);
    if (it == nodeIndex_.end()) {
        return;
    }
    nodes_[it->second].setEnabled(enabled);
}

bool GraphTopology::isNodeEnabled(const std::string& id) const {
    const auto it = nodeIndex_.find(id);
    if (it == nodeIndex_.end()) {
        return true;
    }
    return nodes_[it->second].enabled();
}

bool GraphTopology::connectionExists(const std::string& fromId,
                                     const std::string& toId,
                                     std::uint32_t fromChannel,
                                     std::uint32_t toChannel) const {
    return std::any_of(connections_.begin(), connections_.end(), [&](const GraphConnection& connection) {
        return connection.fromNodeId == fromId && connection.toNodeId == toId &&
            connection.fromChannel == fromChannel && connection.toChannel == toChannel;
    });
}

void GraphTopology::pruneConnectionsForNode(const std::string& id, std::uint32_t inputChannels, std::uint32_t outputChannels) {
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(), [&](const GraphConnection& connection) {
            if (connection.fromNodeId == id && connection.fromChannel >= outputChannels) {
                return true;
            }
            if (connection.toNodeId == id && connection.toChannel >= inputChannels) {
                return true;
            }
            return false;
        }),
        connections_.end());
}

GraphTopology GraphTopology::createDefaultBroadcastLayout() {
    GraphTopology topology;

    auto makeStereoChannels = [](GraphNode& node) {
        node.addInputChannel();
        node.addInputChannel();
        node.addOutputChannel();
        node.addOutputChannel();
    };

    auto addStereoGroup = [&](std::string_view id, std::string_view label) {
        GraphNode groupNode(std::string(id), GraphNodeType::GroupBus);
        groupNode.setLabel(label);
        makeStereoChannels(groupNode);
        topology.addNode(std::move(groupNode));
    };

    addStereoGroup("band_group", "Band Group");
    addStereoGroup("vocal_group", "Vocal Group");
    addStereoGroup("communication_group", "Comms Group");
    addStereoGroup("misc_group", "Misc Group");

    GraphNode broadcastMaster("broadcast_bus", GraphNodeType::BroadcastBus);
    broadcastMaster.setLabel("Broadcast Bus");
    makeStereoChannels(broadcastMaster);
    topology.addNode(std::move(broadcastMaster));

    GraphNode monitorTrim("monitor_trim", GraphNodeType::Utility);
    monitorTrim.setLabel("Monitor Trim -3 dB");
    makeStereoChannels(monitorTrim);
    topology.addNode(std::move(monitorTrim));

    GraphNode broadcastOutput("broadcast_output", GraphNodeType::Output);
    broadcastOutput.setLabel("Broadcast Output");
    broadcastOutput.addInputChannel();
    broadcastOutput.addInputChannel();
    topology.addNode(std::move(broadcastOutput));

    GraphNode utility("utility_channels", GraphNodeType::Utility);
    utility.setLabel("Utility Channels");
    makeStereoChannels(utility);
    topology.addNode(std::move(utility));

    GraphNode monitorBus("monitor_bus", GraphNodeType::MixBus);
    monitorBus.setLabel("Monitor Bus");
    makeStereoChannels(monitorBus);
    topology.addNode(std::move(monitorBus));

    GraphNode monitorOutput("monitor_output", GraphNodeType::Output);
    monitorOutput.setLabel("Monitor Output");
    monitorOutput.addInputChannel();
    monitorOutput.addInputChannel();
    topology.addNode(std::move(monitorOutput));

    const auto connectStereo = [&](std::string_view from, std::string_view to) {
        for (std::uint32_t channel = 0; channel < 2; ++channel) {
            topology.connect(GraphConnection {
                .fromNodeId = std::string(from),
                .fromChannel = channel,
                .toNodeId = std::string(to),
                .toChannel = channel,
            });
        }
    };

    connectStereo("band_group", "broadcast_bus");
    connectStereo("vocal_group", "broadcast_bus");
    connectStereo("communication_group", "broadcast_bus");
    connectStereo("misc_group", "broadcast_bus");
    connectStereo("broadcast_bus", "broadcast_output");
    connectStereo("broadcast_bus", "monitor_trim");
    connectStereo("monitor_trim", "monitor_bus");
    connectStereo("utility_channels", "monitor_bus");
    connectStereo("monitor_bus", "monitor_output");

    return topology;
}

GraphTopology GraphTopology::createGroupMicroLayout(std::string_view groupId) {
    GraphTopology topology;

    const std::string outputId = std::string(groupId) + "_output";
    GraphNode outputNode(outputId, GraphNodeType::Output);
    outputNode.setLabel("Group Output");
    outputNode.addInputChannel();
    outputNode.addInputChannel();
    topology.addNode(std::move(outputNode));

    return topology;
}

GraphTopology GraphTopology::createChannelMicroLayout(std::string_view channelId) {
    GraphTopology topology;

    const std::string inputId = std::string(channelId) + "_input";
    GraphNode inputNode(inputId, GraphNodeType::Input);
    inputNode.setLabel("Channel Input");
    inputNode.addOutputChannel();
    topology.addNode(std::move(inputNode));

    const std::string outputId = std::string(channelId) + "_output";
    GraphNode outputNode(outputId, GraphNodeType::Output);
    outputNode.setLabel("Channel Output");
    outputNode.addInputChannel();
    topology.addNode(std::move(outputNode));

    topology.connect(GraphConnection {
        .fromNodeId = inputId,
        .fromChannel = 0,
        .toNodeId = outputId,
        .toChannel = 0
    });

    return topology;
}

GraphTopology GraphTopology::createOutputMicroLayout(std::string_view outputIdBase) {
    GraphTopology topology;

    const std::string inputId = std::string(outputIdBase) + "_input";
    GraphNode inputNode(inputId, GraphNodeType::Input);
    inputNode.setLabel("Output Input");
    inputNode.addOutputChannel();
    inputNode.addOutputChannel();
    topology.addNode(std::move(inputNode));

    const std::string outputId = std::string(outputIdBase) + "_output";
    GraphNode outputNode(outputId, GraphNodeType::Output);
    outputNode.setLabel("Output");
    outputNode.addInputChannel();
    outputNode.addInputChannel();
    topology.addNode(std::move(outputNode));

    topology.connect(GraphConnection {
        .fromNodeId = inputId,
        .fromChannel = 0,
        .toNodeId = outputId,
        .toChannel = 0
    });
    topology.connect(GraphConnection {
        .fromNodeId = inputId,
        .fromChannel = 1,
        .toNodeId = outputId,
        .toChannel = 1
    });

    return topology;
}

} // namespace broadcastmix::audio
