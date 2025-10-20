#pragma once

#include "GraphTopology.h"
#include "MeterStore.h"

#if BROADCASTMIX_HAS_JUCE
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
#include <unordered_map>
#include <vector>

namespace broadcastmix::audio {

class JuceGraphBuilder {
public:
    JuceGraphBuilder(juce::AudioProcessorGraph& graph, std::shared_ptr<MeterStore> meterStore);

    void rebuildFromTopology(const GraphTopology& topology);

private:
    juce::AudioProcessorGraph& graph_;
    std::shared_ptr<MeterStore> meterStore_;
    std::unordered_map<std::string, juce::AudioProcessorGraph::NodeID> nodeMap_;
    std::optional<juce::AudioProcessorGraph::NodeID> hardwareInputNodeId_;
    std::optional<juce::AudioProcessorGraph::NodeID> hardwareOutputNodeId_;

    std::unique_ptr<juce::AudioProcessor> createProcessorForNode(const GraphNode& node);
};

} // namespace broadcastmix::audio

#endif // BROADCASTMIX_HAS_JUCE
