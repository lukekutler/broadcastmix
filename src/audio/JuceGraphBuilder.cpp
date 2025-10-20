#include "JuceGraphBuilder.h"

#if BROADCASTMIX_HAS_JUCE

#include "processors/GainProcessor.h"
#include "processors/PassThroughProcessor.h"
#include "processors/SignalGeneratorProcessor.h"

#include "../core/Logging.h"

namespace broadcastmix::audio {

namespace {
juce::AudioChannelSet channelSetForNode(const GraphNode& node) {
    const auto channels = static_cast<int>(std::max(node.inputChannelCount(), node.outputChannelCount()));
    if (channels <= 0) {
        return juce::AudioChannelSet::stereo();
    }
    if (channels == 1) {
        return juce::AudioChannelSet::mono();
    }
    if (channels == 2) {
        return juce::AudioChannelSet::stereo();
    }
    return juce::AudioChannelSet::discreteChannels(channels);
}
} // namespace

JuceGraphBuilder::JuceGraphBuilder(juce::AudioProcessorGraph& graph, std::shared_ptr<MeterStore> meterStore)
    : graph_(graph)
    , meterStore_(std::move(meterStore)) {}

void JuceGraphBuilder::rebuildFromTopology(const GraphTopology& topology) {
    graph_.clear();
    nodeMap_.clear();
    hardwareInputNodeId_.reset();
    hardwareOutputNodeId_.reset();

    using IOProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;

    if (auto outputNode = graph_.addNode(std::make_unique<IOProcessor>(IOProcessor::audioOutputNode))) {
        hardwareOutputNodeId_ = outputNode->nodeID;
    }
    if (auto inputNode = graph_.addNode(std::make_unique<IOProcessor>(IOProcessor::audioInputNode))) {
        hardwareInputNodeId_ = inputNode->nodeID;
    }

    struct OutputBinding {
        juce::AudioProcessorGraph::NodeID nodeId;
        std::uint32_t channels;
    };

    struct InputBinding {
        juce::AudioProcessorGraph::NodeID nodeId;
        std::uint32_t channels;
    };

    std::vector<OutputBinding> outputBindings;
    std::vector<InputBinding> inputBindings;

    for (const auto& node : topology.nodes()) {
        auto processor = createProcessorForNode(node);
        if (!processor) {
            core::log(core::LogCategory::Audio, "Failed to create processor for node {}", node.id());
            continue;
        }

        auto nodePtr = graph_.addNode(std::move(processor));
        if (nodePtr == nullptr) {
            core::log(core::LogCategory::Audio, "Failed to add node {}", node.id());
            continue;
        }

        nodeMap_.emplace(node.id(), nodePtr->nodeID);

        const auto channelCount = std::max<std::uint32_t>(1U,
            std::max(node.inputChannelCount(), node.outputChannelCount()));

        switch (node.type()) {
        case GraphNodeType::Output:
            outputBindings.push_back(OutputBinding { nodePtr->nodeID, channelCount });
            break;
        case GraphNodeType::Input:
            inputBindings.push_back(InputBinding { nodePtr->nodeID, channelCount });
            break;
        default:
            break;
        }
    }

    for (const auto& connection : topology.connections()) {
        const auto fromIt = nodeMap_.find(connection.fromNodeId);
        const auto toIt = nodeMap_.find(connection.toNodeId);
        if (fromIt == nodeMap_.end() || toIt == nodeMap_.end()) {
            core::log(core::LogCategory::Audio,
                      "Skipping connection {} -> {} (nodes missing)",
                      connection.fromNodeId,
                      connection.toNodeId);
            continue;
        }

        const auto result = graph_.addConnection({
            { fromIt->second, static_cast<int>(connection.fromChannel) },
            { toIt->second, static_cast<int>(connection.toChannel) },
        });

        if (!result) {
            core::log(core::LogCategory::Audio,
                      "Failed to connect {}:{} -> {}:{}",
                      connection.fromNodeId,
                      connection.fromChannel,
                      connection.toNodeId,
                      connection.toChannel);
        }
    }

    int hardwareOutputChannels = 0;
    if (hardwareOutputNodeId_) {
        if (auto* outputNode = graph_.getNodeForId(*hardwareOutputNodeId_)) {
            hardwareOutputChannels = outputNode->getProcessor() ? outputNode->getProcessor()->getTotalNumInputChannels() : 0;
        }

        for (const auto& binding : outputBindings) {
            const auto channels = static_cast<int>(binding.channels);
            if (channels <= 0) {
                continue;
            }

            for (int channel = 0; channel < channels; ++channel) {
                graph_.addConnection({
                    { binding.nodeId, channel },
                    { *hardwareOutputNodeId_, channel }
                });
            }

            if (channels == 1 && hardwareOutputChannels > 1) {
                for (int extra = 1; extra < hardwareOutputChannels; ++extra) {
                    graph_.addConnection({
                        { binding.nodeId, 0 },
                        { *hardwareOutputNodeId_, extra }
                    });
                }
            }
        }
    }

    if (hardwareInputNodeId_) {
        for (const auto& binding : inputBindings) {
            const auto channels = static_cast<int>(binding.channels);
            for (int channel = 0; channel < channels; ++channel) {
                graph_.addConnection({
                    { *hardwareInputNodeId_, channel },
                    { binding.nodeId, channel }
                });
            }
        }
    }
}

std::unique_ptr<juce::AudioProcessor> JuceGraphBuilder::createProcessorForNode(const GraphNode& node) {
    switch (node.type()) {
    case GraphNodeType::Input:
        return std::make_unique<processors::PassThroughProcessor>(node.label().empty() ? "Input" : node.label(),
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    case GraphNodeType::Output:
        return std::make_unique<processors::PassThroughProcessor>(node.label().empty() ? "Output" : node.label(),
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    case GraphNodeType::SignalGenerator:
        return std::make_unique<processors::SignalGeneratorProcessor>(meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                      channelSetForNode(node));
    case GraphNodeType::Utility:
        if (node.label() == "Monitor Trim -3 dB") {
            return std::make_unique<processors::GainProcessor>(juce::Decibels::decibelsToGain(-3.0F),
                                                               "Monitor Trim -3 dB",
                                                               meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                               channelSetForNode(node));
        }
        return std::make_unique<processors::PassThroughProcessor>("Utility",
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    case GraphNodeType::BroadcastBus:
        return std::make_unique<processors::PassThroughProcessor>("Broadcast Bus",
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    case GraphNodeType::MixBus:
        return std::make_unique<processors::PassThroughProcessor>(node.label().empty() ? "Monitor Bus" : node.label(),
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    case GraphNodeType::GroupBus:
        return std::make_unique<processors::PassThroughProcessor>("Group Bus",
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    case GraphNodeType::Channel:
        return std::make_unique<processors::PassThroughProcessor>("Channel Processing",
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    case GraphNodeType::Plugin:
        return std::make_unique<processors::PassThroughProcessor>("Plugin Placeholder",
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    default:
        return std::make_unique<processors::PassThroughProcessor>("Node",
                                                                  meterStore_ ? meterStore_->meterFor(node.id()) : nullptr,
                                                                  channelSetForNode(node));
    }
}

} // namespace broadcastmix::audio

#endif // BROADCASTMIX_HAS_JUCE
