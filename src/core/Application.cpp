#include "Application.h"

#include "Logging.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>

namespace {

broadcastmix::ui::NodeGraphView::PositionOverrideMap toOverrides(const std::unordered_map<std::string, broadcastmix::persistence::LayoutPosition>& layout) {
    broadcastmix::ui::NodeGraphView::PositionOverrideMap overrides;
    overrides.reserve(layout.size());
    for (const auto& [id, position] : layout) {
        overrides.emplace(id, broadcastmix::ui::NodeGraphView::PositionOverride { position.normX, position.normY });
    }
    return overrides;
}

}

namespace broadcastmix::core {

Application::Application(ApplicationConfig config,
                         audio::AudioEngineSettings audioSettings)
    : config_(std::move(config))
    , audioEngine_(audioSettings) {
    currentProject_.graphTopology = std::make_shared<audio::GraphTopology>();
}

void Application::initialize() {
    log(LogCategory::Lifecycle, "Initializing application {}", config_.version);
    pluginHost_.bootstrap();
    controlManager_.discover();
    nodeGraphView_.loadTheme(ui::UiTheme::createDefault());
    updateService_.initialize(config_.version);
    if (currentProject_.graphTopology) {
        applyMacroLayout();
        applyAudioTopology();
        nodeGraphView_.setTopology(currentProject_.graphTopology);
    }
}

void Application::loadProject(const std::string& path) {
    log(LogCategory::Lifecycle, "Loading project {}", path);
    auto project = projectSerializer_.load(path);
    if (project.graphTopology) {
        nodeCounters_.clear();
        microNodeCounters_.clear();
        currentProject_ = project;
        currentProjectPath_ = path;
        projectLoaded_ = true;

        applyMacroLayout();
        applyAudioTopology();
        nodeGraphView_.setTopology(currentProject_.graphTopology);
    } else {
        nodeCounters_.clear();
        microNodeCounters_.clear();
        currentProject_ = std::move(project);
        currentProjectPath_ = path;
        projectLoaded_ = true;
    }
}

void Application::run() {
    audioEngine_.start();
    nodeGraphView_.runEventLoop();
}

void Application::startRealtimeEngine() {
    audioEngine_.start();
    if (currentProject_.graphTopology) {
        applyMacroLayout();
    }
    nodeGraphView_.setTopology(currentProject_.graphTopology);
}

void Application::stopRealtimeEngine() {
    audioEngine_.stop();
}

audio::AudioEngineStatus Application::audioStatus() const {
    return audioEngine_.status();
}

audio::AudioEngineSettings Application::audioSettings() const {
    return audioEngine_.settings();
}

std::shared_ptr<const audio::GraphTopology> Application::graphTopology() const {
    return currentProject_.graphTopology;
}

ui::NodeGraphView& Application::nodeGraphView() noexcept {
    return nodeGraphView_;
}

const ui::NodeGraphView& Application::nodeGraphView() const noexcept {
    return nodeGraphView_;
}

Application::MicroViewDescriptor Application::microViewDescriptor(const std::string& viewId) {
    return ensureMicroView(viewId);
}

void Application::updateMacroNodePosition(const std::string& nodeId, float normX, float normY) {
    currentProject_.macroLayout[nodeId] = persistence::LayoutPosition { normX, normY };
    if (projectLoaded_) {
        saveProject();
    }
}

void Application::updateMicroNodePosition(const std::string& viewId, const std::string& nodeId, float normX, float normY) {
    auto descriptor = ensureMicroView(viewId);
    currentProject_.microViews[viewId].layout[nodeId] = persistence::LayoutPosition { normX, normY };
    if (projectLoaded_) {
        saveProject();
    }
}

std::array<float, 2> Application::meterLevelForNode(const std::string& nodeId) const {
    if (const auto it = meterAliases_.find(nodeId); it != meterAliases_.end()) {
        return audioEngine_.meterLevelsForNode(it->second);
    }
    return audioEngine_.meterLevelsForNode(nodeId);
}

std::array<float, 2> Application::meterLevelForMicroNode(const std::string& viewId, const std::string& nodeId) const {
    (void) viewId;
    return audioEngine_.meterLevelsForNode(nodeId);
}

const std::unordered_map<std::string, persistence::LayoutPosition>& Application::macroLayout() const noexcept {
    return currentProject_.macroLayout;
}

void Application::applyMacroLayout() {
    nodeGraphView_.setPositionOverrides(toOverrides(currentProject_.macroLayout));
}

void Application::saveProject() {
    if (projectLoaded_ && currentProjectPath_) {
        projectSerializer_.save(currentProject_, *currentProjectPath_);
    }
}

Application::MicroViewDescriptor Application::ensureMicroView(const std::string& viewId) {
    auto& entry = currentProject_.microViews[viewId];
    const bool created = !entry.topology;
    if (created) {
        audio::GraphNodeType nodeType = audio::GraphNodeType::GroupBus;
        if (currentProject_.graphTopology) {
            if (const auto macroNode = currentProject_.graphTopology->findNode(viewId)) {
                nodeType = macroNode->type();
            }
        }

        switch (nodeType) {
        case audio::GraphNodeType::Channel:
            entry.topology = std::make_shared<audio::GraphTopology>(audio::GraphTopology::createChannelMicroLayout(viewId));
            entry.layout[viewId + "_input"] = persistence::LayoutPosition { 0.05F, 0.5F };
            entry.layout[viewId + "_output"] = persistence::LayoutPosition { 0.95F, 0.5F };
            std::cout << "[Application] ensureMicroView channel layout created for " << viewId << std::endl;
            break;
        case audio::GraphNodeType::GroupBus:
            entry.topology = std::make_shared<audio::GraphTopology>(audio::GraphTopology::createGroupMicroLayout(viewId));
            entry.layout[viewId + "_output"] = persistence::LayoutPosition { 0.95F, 0.5F };
            break;
        case audio::GraphNodeType::Output:
            entry.topology = std::make_shared<audio::GraphTopology>(audio::GraphTopology::createOutputMicroLayout(viewId));
            entry.layout[viewId + "_input"] = persistence::LayoutPosition { 0.05F, 0.5F };
            entry.layout[viewId + "_output"] = persistence::LayoutPosition { 0.95F, 0.5F };
            break;
        case audio::GraphNodeType::Plugin:
            entry.topology.reset();
            entry.layout.clear();
            break;
        default:
            entry.topology = std::make_shared<audio::GraphTopology>();
            break;
        }
    }

    if (entry.topology) {
        updateMicroTopologyForNode(viewId);
        applyAudioTopology();
    }

    MicroViewDescriptor descriptor;
    descriptor.topology = entry.topology;
    descriptor.layout = entry.layout;

    if (created && projectLoaded_) {
        saveProject();
    }

    return descriptor;
}

bool Application::deleteNode(const std::string& nodeId) {
    core::log(LogCategory::Ui, "deleteNode requested for {}", nodeId);
    if (!currentProject_.graphTopology) {
        core::log(LogCategory::Ui, "deleteNode aborted: no topology loaded");
        return false;
    }

    const auto node = currentProject_.graphTopology->findNode(nodeId);
    if (!node) {
        core::log(LogCategory::Ui, "deleteNode aborted: node {} not found", nodeId);
        return false;
    }

    const auto topology = currentProject_.graphTopology;
    const auto nodeTemplate = templateForGraphType(node->type());
    std::vector<audio::GraphConnection> incomingConnections;
    std::vector<audio::GraphConnection> outgoingConnections;
    incomingConnections.reserve(topology->connections().size());
    outgoingConnections.reserve(topology->connections().size());

    for (const auto& connection : topology->connections()) {
        if (connection.toNodeId == nodeId) {
            incomingConnections.push_back(connection);
        } else if (connection.fromNodeId == nodeId) {
            outgoingConnections.push_back(connection);
        }
    }

    currentProject_.graphTopology->removeNode(nodeId);
    currentProject_.macroLayout.erase(nodeId);
    currentProject_.microViews.erase(nodeId);

    for (const auto& incoming : incomingConnections) {
        for (const auto& outgoing : outgoingConnections) {
            if (incoming.toChannel != outgoing.fromChannel) {
                continue;
            }
            if (incoming.fromNodeId == outgoing.toNodeId) {
                continue;
            }
            if (topology->connectionExists(incoming.fromNodeId, outgoing.toNodeId,
                    incoming.fromChannel, outgoing.toChannel)) {
                continue;
            }

            topology->connect(audio::GraphConnection {
                .fromNodeId = incoming.fromNodeId,
                .fromChannel = incoming.fromChannel,
                .toNodeId = outgoing.toNodeId,
                .toChannel = outgoing.toChannel
            });
        }
    }

    core::log(LogCategory::Ui, "deleteNode succeeded for {}", nodeId);
    if (nodeTemplate) {
        renumberMacroNodes(*nodeTemplate);
    }
    applyMacroLayout();
    applyAudioTopology();
    nodeGraphView_.setTopology(currentProject_.graphTopology);
    saveProject();
    return true;
}

bool Application::toggleNodeEnabled(const std::string& nodeId) {
    core::log(LogCategory::Ui, "toggleNode requested for {}", nodeId);
    if (!currentProject_.graphTopology) {
        core::log(LogCategory::Ui, "toggleNode aborted: no topology loaded");
        return false;
    }

    if (!currentProject_.graphTopology->findNode(nodeId)) {
        core::log(LogCategory::Ui, "toggleNode aborted: node {} not found", nodeId);
        return false;
    }

    const bool currentlyEnabled = currentProject_.graphTopology->isNodeEnabled(nodeId);
    currentProject_.graphTopology->setNodeEnabled(nodeId, !currentlyEnabled);
    core::log(LogCategory::Ui, "toggleNode completed for {} -> {}", nodeId, (!currentlyEnabled ? "enabled" : "disabled"));
    applyAudioTopology();
    nodeGraphView_.setTopology(currentProject_.graphTopology);
    saveProject();
    return true;
}

bool Application::connectNodes(const std::string& fromId, const std::string& toId) {
    if (!currentProject_.graphTopology) {
        return false;
    }

    if (!currentProject_.graphTopology->findNode(fromId) || !currentProject_.graphTopology->findNode(toId) || fromId == toId) {
        return false;
    }

    bool updated = false;
    for (std::uint32_t channel = 0; channel < 2; ++channel) {
        if (!currentProject_.graphTopology->connectionExists(fromId, toId, channel, channel)) {
            currentProject_.graphTopology->connect(audio::GraphConnection {
                .fromNodeId = fromId,
                .fromChannel = channel,
                .toNodeId = toId,
                .toChannel = channel
            });
            updated = true;
        }
    }

    if (updated) {
        applyAudioTopology();
        nodeGraphView_.setTopology(currentProject_.graphTopology);
        saveProject();
    }

    return updated;
}

bool Application::disconnectNodes(const std::string& fromId, const std::string& toId) {
    if (!currentProject_.graphTopology) {
        return false;
    }

    if (!currentProject_.graphTopology->findNode(fromId) || !currentProject_.graphTopology->findNode(toId) || fromId == toId) {
        return false;
    }

    bool updated = false;
    for (std::uint32_t channel = 0; channel < 2; ++channel) {
        if (currentProject_.graphTopology->connectionExists(fromId, toId, channel, channel)) {
            currentProject_.graphTopology->disconnect(fromId, toId);
            updated = true;
            break;
        }
    }

    if (updated) {
        applyAudioTopology();
        nodeGraphView_.setTopology(currentProject_.graphTopology);
        saveProject();
    }

    return updated;
}

bool Application::connectNodePorts(const std::string& fromId, std::size_t fromChannel, const std::string& toId, std::size_t toChannel) {
    log(LogCategory::Ui, "connectNodePorts {}:{} -> {}:{}", fromId, fromChannel, toId, toChannel);
    if (!currentProject_.graphTopology) {
        return false;
    }

    if (fromId == toId) {
        return false;
    }

    const auto fromNode = currentProject_.graphTopology->findNode(fromId);
    const auto toNode = currentProject_.graphTopology->findNode(toId);
    if (!fromNode || !toNode) {
        return false;
    }

    if (fromChannel >= fromNode->outputChannelCount() || toChannel >= toNode->inputChannelCount()) {
        return false;
    }

    if (currentProject_.graphTopology->connectionExists(fromId, toId,
            static_cast<std::uint32_t>(fromChannel), static_cast<std::uint32_t>(toChannel))) {
        return false;
    }

    currentProject_.graphTopology->connect(audio::GraphConnection {
        .fromNodeId = fromId,
        .fromChannel = static_cast<std::uint32_t>(fromChannel),
        .toNodeId = toId,
        .toChannel = static_cast<std::uint32_t>(toChannel)
    });

    applyAudioTopology();
    nodeGraphView_.setTopology(currentProject_.graphTopology);
    saveProject();
    return true;
}

bool Application::deleteMicroNode(const std::string& viewId, const std::string& nodeId) {
    auto it = currentProject_.microViews.find(viewId);
    if (it == currentProject_.microViews.end() || !it->second.topology) {
        return false;
    }

    auto& state = it->second;
    if (!state.topology->findNode(nodeId)) {
        return false;
    }

    std::vector<audio::GraphConnection> incomingConnections;
    std::vector<audio::GraphConnection> outgoingConnections;
    incomingConnections.reserve(state.topology->connections().size());
    outgoingConnections.reserve(state.topology->connections().size());

    for (const auto& connection : state.topology->connections()) {
        if (connection.toNodeId == nodeId) {
            incomingConnections.push_back(connection);
        } else if (connection.fromNodeId == nodeId) {
            outgoingConnections.push_back(connection);
        }
    }

    state.topology->removeNode(nodeId);
    state.layout.erase(nodeId);

    for (const auto& incoming : incomingConnections) {
        for (const auto& outgoing : outgoingConnections) {
            if (incoming.toChannel != outgoing.fromChannel) {
                continue;
            }
            if (incoming.fromNodeId == outgoing.toNodeId) {
                continue;
            }
            if (state.topology->connectionExists(incoming.fromNodeId, outgoing.toNodeId,
                    incoming.fromChannel, outgoing.toChannel)) {
                continue;
            }

            state.topology->connect(audio::GraphConnection {
                .fromNodeId = incoming.fromNodeId,
                .fromChannel = incoming.fromChannel,
                .toNodeId = outgoing.toNodeId,
                .toChannel = outgoing.toChannel
            });
        }
    }

    applyAudioTopology();
    saveProject();
    return true;
}

bool Application::toggleMicroNodeEnabled(const std::string& viewId, const std::string& nodeId) {
    auto it = currentProject_.microViews.find(viewId);
    if (it == currentProject_.microViews.end() || !it->second.topology) {
        return false;
    }

    auto& state = it->second;
    if (!state.topology->findNode(nodeId)) {
        return false;
    }

    const bool enabled = state.topology->isNodeEnabled(nodeId);
    state.topology->setNodeEnabled(nodeId, !enabled);
    applyAudioTopology();
    saveProject();
    return true;
}

bool Application::connectMicroNodePorts(const std::string& viewId,
                                        const std::string& fromId,
                                        std::size_t fromChannel,
                                        const std::string& toId,
                                        std::size_t toChannel) {
    log(LogCategory::Ui, "connectMicroNodePorts {}:{} -> {}:{} in {}", fromId, fromChannel, toId, toChannel, viewId);
    ensureMicroView(viewId);
    auto it = currentProject_.microViews.find(viewId);
    if (it == currentProject_.microViews.end() || !it->second.topology) {
        return false;
    }

    if (fromId == toId) {
        return false;
    }

    auto& state = it->second;
    const auto fromNode = state.topology->findNode(fromId);
    const auto toNode = state.topology->findNode(toId);
    if (!fromNode || !toNode) {
        return false;
    }

    if (fromChannel >= fromNode->outputChannelCount() || toChannel >= toNode->inputChannelCount()) {
        return false;
    }

    if (state.topology->connectionExists(fromId, toId,
            static_cast<std::uint32_t>(fromChannel), static_cast<std::uint32_t>(toChannel))) {
        return false;
    }

    state.topology->connect(audio::GraphConnection {
        .fromNodeId = fromId,
        .fromChannel = static_cast<std::uint32_t>(fromChannel),
        .toNodeId = toId,
        .toChannel = static_cast<std::uint32_t>(toChannel)
    });

    applyAudioTopology();
    saveProject();
    return true;
}

bool Application::connectMicroNodes(const std::string& viewId, const std::string& fromId, const std::string& toId) {
    auto it = currentProject_.microViews.find(viewId);
    if (it == currentProject_.microViews.end() || !it->second.topology) {
        return false;
    }

    auto& state = it->second;
    if (!state.topology->findNode(fromId) || !state.topology->findNode(toId) || fromId == toId) {
        return false;
    }

    bool updated = false;
    for (std::uint32_t channel = 0; channel < 2; ++channel) {
        if (!state.topology->connectionExists(fromId, toId, channel, channel)) {
            state.topology->connect(audio::GraphConnection {
                .fromNodeId = fromId,
                .fromChannel = channel,
                .toNodeId = toId,
                .toChannel = channel
            });
            updated = true;
        }
    }

    if (updated) {
        applyAudioTopology();
        saveProject();
    }

    return updated;
}

bool Application::disconnectMicroNodes(const std::string& viewId, const std::string& fromId, const std::string& toId) {
    auto it = currentProject_.microViews.find(viewId);
    if (it == currentProject_.microViews.end() || !it->second.topology) {
        return false;
    }

    auto& state = it->second;
    if (!state.topology->findNode(fromId) || !state.topology->findNode(toId) || fromId == toId) {
        return false;
    }

    bool updated = false;
    for (std::uint32_t channel = 0; channel < 2; ++channel) {
        if (state.topology->connectionExists(fromId, toId, channel, channel)) {
            state.topology->disconnect(fromId, toId);
            updated = true;
            break;
        }
    }

    if (updated) {
        applyAudioTopology();
        saveProject();
    }

    return updated;
}

std::string Application::templatePrefix(NodeTemplate type) const {
    switch (type) {
    case NodeTemplate::Channel:
        return "channel";
    case NodeTemplate::Output:
        return "output";
    case NodeTemplate::Group:
        return "group";
    case NodeTemplate::Effect:
        return "effect";
    case NodeTemplate::SignalGenerator:
        return "signal";
    default:
        return "node";
    }
}

audio::GraphNodeType Application::graphTypeForTemplate(NodeTemplate type) const {
    switch (type) {
    case NodeTemplate::Channel:
        return audio::GraphNodeType::Channel;
    case NodeTemplate::Output:
        return audio::GraphNodeType::Output;
    case NodeTemplate::Group:
        return audio::GraphNodeType::GroupBus;
    case NodeTemplate::Effect:
        return audio::GraphNodeType::Plugin;
    case NodeTemplate::SignalGenerator:
        return audio::GraphNodeType::SignalGenerator;
    default:
        return audio::GraphNodeType::Utility;
    }
}

std::optional<Application::NodeTemplate> Application::templateForGraphType(audio::GraphNodeType type) const {
    switch (type) {
    case audio::GraphNodeType::Channel:
        return NodeTemplate::Channel;
    case audio::GraphNodeType::Output:
        return NodeTemplate::Output;
    case audio::GraphNodeType::GroupBus:
        return NodeTemplate::Group;
    case audio::GraphNodeType::Plugin:
        return NodeTemplate::Effect;
    case audio::GraphNodeType::SignalGenerator:
        return NodeTemplate::SignalGenerator;
    default:
        return std::nullopt;
    }
}

void Application::configureChannelsForTemplate(audio::GraphNode& node, NodeTemplate type) const {
    const auto addStereoInputs = [&]() {
        node.addInputChannel();
        node.addInputChannel();
    };
    const auto addStereoOutputs = [&]() {
        node.addOutputChannel();
        node.addOutputChannel();
    };

    switch (type) {
    case NodeTemplate::Channel:
        addStereoInputs();
        addStereoOutputs();
        break;
    case NodeTemplate::Output:
        addStereoInputs();
        break;
    case NodeTemplate::Group:
    case NodeTemplate::Effect:
        addStereoInputs();
        addStereoOutputs();
        break;
    case NodeTemplate::SignalGenerator:
        addStereoInputs();
        addStereoOutputs();
        break;
    default:
        addStereoOutputs();
        break;
    }
}

std::string Application::labelBase(NodeTemplate type) const {
    std::string base;
    switch (type) {
    case NodeTemplate::Channel:
        base = "Channel";
        break;
    case NodeTemplate::Output:
        base = "Output";
        break;
    case NodeTemplate::Group:
        base = "Group";
        break;
    case NodeTemplate::Effect:
        base = "Effect";
        break;
    case NodeTemplate::SignalGenerator:
        base = "Signal Generator";
        break;
    default:
        base = "Node";
        break;
    }
    return base;
}

std::string Application::makeLabel(NodeTemplate type, std::size_t index) const {
    return labelBase(type) + " " + std::to_string(index);
}

std::string Application::nextNodeId(NodeTemplate type) {
    const auto prefix = templatePrefix(type);
    auto& counter = nodeCounters_[prefix];
    std::string candidate;

    do {
        ++counter;
        candidate = prefix + "_" + std::to_string(counter);
    } while (currentProject_.graphTopology && currentProject_.graphTopology->findNode(candidate));

    return candidate;
}

std::string Application::nextMicroNodeId(const std::string& viewId, NodeTemplate type, audio::GraphTopology& topology) {
    const auto prefix = templatePrefix(type);
    const auto counterKey = viewId + ":" + prefix;
    auto& counter = microNodeCounters_[counterKey];
    std::string candidate;

    do {
        ++counter;
        candidate = prefix + "_" + std::to_string(counter);
    } while (topology.findNode(candidate));

    return candidate;
}

void Application::renumberMacroNodes(NodeTemplate type) {
    if (!currentProject_.graphTopology) {
        return;
    }

    std::vector<std::string> ids;
    ids.reserve(currentProject_.graphTopology->nodes().size());
    for (const auto& node : currentProject_.graphTopology->nodes()) {
        if (const auto templ = templateForGraphType(node.type()); templ && *templ == type) {
            ids.push_back(node.id());
        }
    }

    std::sort(ids.begin(), ids.end());
    std::size_t index = 1;
    for (const auto& id : ids) {
        currentProject_.graphTopology->setNodeLabel(id, makeLabel(type, index++));
    }
}

std::uint32_t Application::channelCountForMicroInsertion(const audio::GraphTopology& topology,
                                                         const std::optional<std::pair<std::string, std::string>>& insertBetween) const {
    constexpr std::uint32_t kDefaultChannels = 2;
    if (!insertBetween) {
        return kDefaultChannels;
    }

    const auto upstream = topology.findNode(insertBetween->first);
    const auto downstream = topology.findNode(insertBetween->second);
    const auto upstreamOutputs = upstream ? upstream->outputChannelCount() : kDefaultChannels;
    const auto downstreamInputs = downstream ? downstream->inputChannelCount() : kDefaultChannels;
    const auto channels = std::max<std::uint32_t>(1U, std::max(upstreamOutputs, downstreamInputs));
    return std::max<std::uint32_t>(1U, std::min<std::uint32_t>(2U, channels));
}

void Application::updateMicroTopologyForNode(const std::string& nodeId) {
    if (!currentProject_.graphTopology) {
        return;
    }

    const auto macroNodeOpt = currentProject_.graphTopology->findNode(nodeId);
    if (!macroNodeOpt) {
        return;
    }

    auto microIt = currentProject_.microViews.find(nodeId);
    if (microIt == currentProject_.microViews.end() || !microIt->second.topology) {
        return;
    }

    auto& microTopology = *microIt->second.topology;
    const auto nodeType = macroNodeOpt->type();

    const auto inputId = nodeId + "_input";
    const auto outputId = nodeId + "_output";

    const auto clampChannels = [](std::uint32_t channels) {
        return std::max<std::uint32_t>(1U, std::min<std::uint32_t>(channels, 2U));
    };

    const auto ensureDirectConnection = [&](const std::string& fromId, const std::string& toId) {
        const auto fromNode = microTopology.findNode(fromId);
        const auto toNode = microTopology.findNode(toId);
        const auto fromChannels = clampChannels(fromNode ? fromNode->outputChannelCount() : 1U);
        const auto toChannels = clampChannels(toNode ? toNode->inputChannelCount() : 1U);

        microTopology.disconnect(fromId, toId);

        if (fromChannels == 0 || toChannels == 0) {
            return;
        }

        if (fromChannels >= toChannels) {
            for (std::uint32_t channel = 0; channel < toChannels; ++channel) {
                microTopology.connect(audio::GraphConnection {
                    .fromNodeId = fromId,
                    .fromChannel = channel,
                    .toNodeId = toId,
                    .toChannel = channel
                });
            }
        } else if (fromChannels == 1) {
            for (std::uint32_t channel = 0; channel < toChannels; ++channel) {
                microTopology.connect(audio::GraphConnection {
                    .fromNodeId = fromId,
                    .fromChannel = 0,
                    .toNodeId = toId,
                    .toChannel = channel
                });
            }
        } else {
            for (std::uint32_t channel = 0; channel < toChannels; ++channel) {
                const auto sourceChannel = std::min(channel, fromChannels - 1);
                microTopology.connect(audio::GraphConnection {
                    .fromNodeId = fromId,
                    .fromChannel = sourceChannel,
                    .toNodeId = toId,
                    .toChannel = channel
                });
            }
        }
    };

    switch (nodeType) {
    case audio::GraphNodeType::Channel: {
        const auto inChannels = clampChannels(macroNodeOpt->inputChannelCount());
        const auto outChannels = clampChannels(macroNodeOpt->outputChannelCount());
        microTopology.setNodeChannelCounts(inputId, 0, inChannels);
        microTopology.setNodeChannelCounts(outputId, outChannels, 0);
        const auto& nodes = microTopology.nodes();
        const bool hasInlineNodes = std::any_of(nodes.begin(), nodes.end(), [&](const audio::GraphNode& node) {
            return node.id() != inputId && node.id() != outputId;
        });
        if (hasInlineNodes) {
            microTopology.disconnect(inputId, outputId);
        } else {
            ensureDirectConnection(inputId, outputId);
        }
        break;
    }
    case audio::GraphNodeType::Output: {
        const auto inChannels = clampChannels(macroNodeOpt->inputChannelCount());
        microTopology.setNodeChannelCounts(inputId, 0, inChannels);
        microTopology.setNodeChannelCounts(outputId, inChannels, 0);
        const auto& nodes = microTopology.nodes();
        const bool hasInlineNodes = std::any_of(nodes.begin(), nodes.end(), [&](const audio::GraphNode& node) {
            return node.id() != inputId && node.id() != outputId;
        });
        if (hasInlineNodes) {
            microTopology.disconnect(inputId, outputId);
        } else {
            ensureDirectConnection(inputId, outputId);
        }
        break;
    }
    case audio::GraphNodeType::GroupBus: {
        const auto outChannels = clampChannels(macroNodeOpt->outputChannelCount());
        microTopology.setNodeChannelCounts(outputId, outChannels, 0);
        break;
    }
    default:
        break;
    }
}

bool Application::rewireForInsertion(audio::GraphTopology& topology,
                                     const std::optional<std::pair<std::string, std::string>>& insertBetween,
                                     const std::string& newNodeId,
                                     std::uint32_t newInputChannels,
                                     std::uint32_t newOutputChannels) {
    if (!insertBetween || newInputChannels == 0 || newOutputChannels == 0) {
        return false;
    }

    const auto& [fromId, toId] = *insertBetween;
    const auto fromNode = topology.findNode(fromId);
    const auto toNode = topology.findNode(toId);
    if (!fromNode || !toNode) {
        return false;
    }

    const auto upstreamOutputs = fromNode->outputChannelCount();
    const auto downstreamInputs = toNode->inputChannelCount();
    if (upstreamOutputs == 0 || downstreamInputs == 0) {
        return false;
    }

    topology.disconnect(fromId, toId);

    const auto upstreamChannels = std::min({ upstreamOutputs, newInputChannels, std::uint32_t(2) });
    const auto downstreamChannels = std::min({ newOutputChannels, downstreamInputs, std::uint32_t(2) });

    bool upstreamConnected = false;
    bool downstreamConnected = false;

    for (std::uint32_t channel = 0; channel < upstreamChannels; ++channel) {
        topology.connect(audio::GraphConnection {
            .fromNodeId = fromId,
            .fromChannel = channel,
            .toNodeId = newNodeId,
            .toChannel = channel
        });
        upstreamConnected = true;
    }

    for (std::uint32_t channel = 0; channel < downstreamChannels; ++channel) {
        topology.connect(audio::GraphConnection {
            .fromNodeId = newNodeId,
            .fromChannel = channel,
            .toNodeId = toId,
            .toChannel = channel
        });
        downstreamConnected = true;
    }

    if (!upstreamConnected || !downstreamConnected) {
        const auto restoreChannels = std::min({ upstreamOutputs, downstreamInputs, std::uint32_t(2) });
        for (std::uint32_t channel = 0; channel < restoreChannels; ++channel) {
            topology.connect(audio::GraphConnection {
                .fromNodeId = fromId,
                .fromChannel = channel,
                .toNodeId = toId,
                .toChannel = channel
            });
        }
        return false;
    }

    return true;
}

void Application::detachNodeConnections(audio::GraphTopology& topology,
                                        const std::string& nodeId,
                                        std::vector<audio::GraphConnection>& removedConnections) {
    removedConnections.clear();
    for (const auto& connection : topology.connections()) {
        if (connection.fromNodeId == nodeId || connection.toNodeId == nodeId) {
            removedConnections.push_back(connection);
        }
    }

    std::vector<std::pair<std::string, std::string>> pairs;
    pairs.reserve(removedConnections.size());
    for (const auto& connection : removedConnections) {
        pairs.emplace_back(connection.fromNodeId, connection.toNodeId);
    }
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    for (const auto& pair : pairs) {
        topology.disconnect(pair.first, pair.second);
    }
}

void Application::restoreConnections(audio::GraphTopology& topology,
                                      const std::vector<audio::GraphConnection>& connections) {
    for (const auto& connection : connections) {
        topology.connect(connection);
    }
}

std::shared_ptr<audio::GraphTopology> Application::buildAudioTopology() const {
    meterAliases_.clear();

    auto composite = std::make_shared<audio::GraphTopology>();
    if (!currentProject_.graphTopology) {
        return composite;
    }

    std::unordered_map<std::string, audio::GraphNode> macroNodes;
    macroNodes.reserve(currentProject_.graphTopology->nodes().size());
    for (const auto& node : currentProject_.graphTopology->nodes()) {
        macroNodes.emplace(node.id(), node);
    }

    std::unordered_map<std::string, std::string> microInputNodes;
    std::unordered_map<std::string, std::string> microOutputNodes;

    struct MicroEndpoints {
        bool hasInput { false };
        bool hasOutput { false };
    };

    std::unordered_map<std::string, MicroEndpoints> microEndpoints;
    microEndpoints.reserve(currentProject_.microViews.size());
    for (const auto& [macroId, state] : currentProject_.microViews) {
        if (!state.topology) {
            continue;
        }
        MicroEndpoints endpoints;
        for (const auto& node : state.topology->nodes()) {
            if (node.type() == audio::GraphNodeType::Input) {
                endpoints.hasInput = true;
            } else if (node.type() == audio::GraphNodeType::Output) {
                endpoints.hasOutput = true;
            }
        }
        microEndpoints.emplace(macroId, endpoints);
    }

    const auto shouldInline = [&](const std::string& nodeId) {
        const auto endpointIt = microEndpoints.find(nodeId);
        if (endpointIt == microEndpoints.end()) {
            return false;
        }

        const auto macroIt = macroNodes.find(nodeId);
        const auto nodeType = macroIt != macroNodes.end() ? macroIt->second.type() : audio::GraphNodeType::Utility;
        const auto& endpoints = endpointIt->second;

        switch (nodeType) {
        case audio::GraphNodeType::Channel:
        case audio::GraphNodeType::Output:
            return endpoints.hasInput && endpoints.hasOutput;
        case audio::GraphNodeType::GroupBus:
            return endpoints.hasInput && endpoints.hasOutput;
        default:
            return endpoints.hasInput && endpoints.hasOutput;
        }
    };

    std::unordered_set<std::string> inlinedMacros;

    for (const auto& node : currentProject_.graphTopology->nodes()) {
        if (shouldInline(node.id())) {
            inlinedMacros.insert(node.id());
            continue;
        }
        composite->addNode(node);
    }

    const auto cloneNodeWithChannels = [](const audio::GraphNode& source,
                                          audio::GraphNodeType type,
                                          std::uint32_t inputs,
                                          std::uint32_t outputs) {
        audio::GraphNode clone(source.id(), type);
        clone.setLabel(source.label());
        clone.setEnabled(source.enabled());
        for (std::uint32_t i = 0; i < inputs; ++i) {
            clone.addInputChannel();
        }
        for (std::uint32_t i = 0; i < outputs; ++i) {
            clone.addOutputChannel();
        }
        return clone;
    };

    for (const auto& [macroId, state] : currentProject_.microViews) {
        if (!state.topology) {
            continue;
        }

        const auto macroIt = macroNodes.find(macroId);
        const auto macroInputs = macroIt != macroNodes.end() ? macroIt->second.inputChannelCount() : 1U;
        const auto macroOutputs = macroIt != macroNodes.end() ? macroIt->second.outputChannelCount() : 1U;

        for (const auto& node : state.topology->nodes()) {
            audio::GraphNode clone = node;
            switch (node.type()) {
            case audio::GraphNodeType::Input: {
                const auto channelCount = std::max<std::uint32_t>(1U, macroInputs);
                clone = cloneNodeWithChannels(node, audio::GraphNodeType::Utility, channelCount, channelCount);
                microInputNodes[macroId] = node.id();
                break;
            }
            case audio::GraphNodeType::Output: {
                const auto channelCount = std::max<std::uint32_t>(1U, macroOutputs);
                clone = cloneNodeWithChannels(node, audio::GraphNodeType::Utility, channelCount, channelCount);
                microOutputNodes[macroId] = node.id();
                meterAliases_[macroId] = node.id();
                break;
            }
            default: {
                clone = cloneNodeWithChannels(node, node.type(), node.inputChannelCount(), node.outputChannelCount());
                break;
            }
            }

            if (!composite->findNode(clone.id())) {
                composite->addNode(std::move(clone));
            }
        }

        for (const auto& connection : state.topology->connections()) {
            composite->connect(connection);
        }
    }

    for (const auto& connection : currentProject_.graphTopology->connections()) {
        auto fromId = connection.fromNodeId;
        auto toId = connection.toNodeId;

        if (const auto outIt = microOutputNodes.find(fromId); outIt != microOutputNodes.end()) {
            fromId = outIt->second;
        }
        if (const auto inIt = microInputNodes.find(toId); inIt != microInputNodes.end()) {
            toId = inIt->second;
        }

        composite->connect(audio::GraphConnection {
            .fromNodeId = fromId,
            .fromChannel = connection.fromChannel,
            .toNodeId = toId,
            .toChannel = connection.toChannel
        });

        const auto fromNode = composite->findNode(fromId);
        const auto toNode = composite->findNode(toId);
        const auto fromChannels = fromNode ? std::max<std::uint32_t>(1U, fromNode->outputChannelCount()) : 1U;
        const auto toChannels = toNode ? std::max<std::uint32_t>(1U, toNode->inputChannelCount()) : 1U;

        if (fromChannels == 1 && toChannels > 1) {
            for (std::uint32_t channel = 1; channel < toChannels; ++channel) {
                if (!composite->connectionExists(fromId, toId, 0, channel)) {
                    composite->connect(audio::GraphConnection {
                        .fromNodeId = fromId,
                        .fromChannel = 0,
                        .toNodeId = toId,
                        .toChannel = channel
                    });
                }
            }
        } else if (fromChannels > 1 && toChannels == 1) {
            for (std::uint32_t channel = 1; channel < fromChannels; ++channel) {
                if (!composite->connectionExists(fromId, toId, channel, 0)) {
                    composite->connect(audio::GraphConnection {
                        .fromNodeId = fromId,
                        .fromChannel = channel,
                        .toNodeId = toId,
                        .toChannel = 0
                    });
                }
            }
        }
    }

    for (const auto& [macroId, microOutputId] : microOutputNodes) {
        if (inlinedMacros.contains(macroId)) {
            continue;
        }
        if (!composite->findNode(macroId)) {
            continue;
        }
        const auto macroIt = macroNodes.find(macroId);
        if (macroIt == macroNodes.end()) {
            continue;
        }
        const auto channels = std::max<std::uint32_t>(1U, std::min<std::uint32_t>(macroIt->second.outputChannelCount(), 2U));
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            composite->connect(audio::GraphConnection {
                .fromNodeId = macroId,
                .fromChannel = channel,
                .toNodeId = microOutputId,
                .toChannel = channel
            });
        }
    }

    return composite;
}

void Application::applyAudioTopology() {
    audioEngine_.setTopology(buildAudioTopology());
}

bool Application::createNode(NodeTemplate type,
                             float normX,
                             float normY,
                             std::optional<std::pair<std::string, std::string>> insertBetween) {
    if (!currentProject_.graphTopology) {
        currentProject_.graphTopology = std::make_shared<audio::GraphTopology>();
    }

    const auto id = nextNodeId(type);
    const auto prefix = templatePrefix(type);
    const auto iteration = nodeCounters_[prefix];

    audio::GraphNode node(id, graphTypeForTemplate(type));
    configureChannelsForTemplate(node, type);
    node.setLabel(makeLabel(type, iteration));

    const auto newInputChannels = node.inputChannelCount();
    const auto newOutputChannels = node.outputChannelCount();

    currentProject_.graphTopology->addNode(std::move(node));
    currentProject_.macroLayout[id] = persistence::LayoutPosition {
        .normX = std::clamp(normX, 0.0F, 1.0F),
        .normY = std::clamp(normY, 0.0F, 1.0F)
    };

    log(LogCategory::Ui, "createNode {} id={} @({}, {})", prefix, id, normX, normY);

    if (!rewireForInsertion(*currentProject_.graphTopology, insertBetween, id, newInputChannels, newOutputChannels)) {
        if (insertBetween) {
            log(LogCategory::Ui, "createNode insertion fallback for {} between {} -> {}", id, insertBetween->first, insertBetween->second);
        }
    }

    renumberMacroNodes(type);
    applyMacroLayout();
    applyAudioTopology();
    nodeGraphView_.setTopology(currentProject_.graphTopology);
    saveProject();
    return true;
}

bool Application::createMicroNode(const std::string& viewId,
                                  NodeTemplate type,
                                  float normX,
                                  float normY,
                                  std::optional<std::pair<std::string, std::string>> insertBetween) {
    ensureMicroView(viewId);
    auto& state = currentProject_.microViews[viewId];
    if (!state.topology) {
        return false;
    }

    const auto id = nextMicroNodeId(viewId, type, *state.topology);
    const auto prefix = templatePrefix(type);
    const auto counterKey = viewId + ":" + prefix;
    const auto iteration = microNodeCounters_[counterKey];

    audio::GraphNode node(id, graphTypeForTemplate(type));
    if (type == NodeTemplate::SignalGenerator) {
        std::uint32_t channels = channelCountForMicroInsertion(*state.topology, insertBetween);
        if (currentProject_.graphTopology) {
            if (const auto macroNode = currentProject_.graphTopology->findNode(viewId)) {
                const auto macroChannels = std::max<std::uint32_t>(macroNode->inputChannelCount(), macroNode->outputChannelCount());
                channels = std::max(channels, std::min<std::uint32_t>(macroChannels, 2U));
            }
        }
        channels = std::max<std::uint32_t>(channels, 1U);
        for (std::uint32_t i = 0; i < channels; ++i) {
            node.addInputChannel();
            node.addOutputChannel();
        }
    } else {
        configureChannelsForTemplate(node, type);
    }
    node.setLabel(makeLabel(type, iteration));

    const auto newInputChannels = node.inputChannelCount();
    const auto newOutputChannels = node.outputChannelCount();

    state.topology->addNode(std::move(node));
    state.layout[id] = persistence::LayoutPosition {
        .normX = std::clamp(normX, 0.0F, 1.0F),
        .normY = std::clamp(normY, 0.0F, 1.0F)
    };

    std::cout << "[Application] createMicroNode type=" << static_cast<int>(type)
              << " id=" << id << " view=" << viewId
              << " normX=" << normX << " normY=" << normY << std::endl;

    if (!rewireForInsertion(*state.topology, insertBetween, id, newInputChannels, newOutputChannels)) {
        if (insertBetween) {
            log(LogCategory::Ui, "createMicroNode insertion fallback for {} between {} -> {} in {}", id, insertBetween->first, insertBetween->second, viewId);
        }
    }

    applyAudioTopology();
    saveProject();
    return true;
}

bool Application::swapMacroNodes(const std::string& first, const std::string& second) {
    if (!currentProject_.graphTopology) {
        return false;
    }

    const auto findPosition = [&](const std::string& nodeId) {
        if (const auto it = currentProject_.macroLayout.find(nodeId); it != currentProject_.macroLayout.end()) {
            return it->second;
        }
        for (const auto& node : nodeGraphView_.nodes()) {
            if (node.id == nodeId) {
                return persistence::LayoutPosition { node.normX, node.normY };
            }
        }
        return persistence::LayoutPosition { 0.5F, 0.5F };
    };

    const auto firstPos = findPosition(first);
    const auto secondPos = findPosition(second);

    currentProject_.macroLayout[first] = secondPos;
    currentProject_.macroLayout[second] = firstPos;

    applyMacroLayout();
    saveProject();
    log(LogCategory::Ui, "swapMacroNodes {} <-> {}", first, second);
    return true;
}

bool Application::swapMicroNodes(const std::string& viewId, const std::string& first, const std::string& second) {
    auto it = currentProject_.microViews.find(viewId);
    if (it == currentProject_.microViews.end() || !it->second.topology) {
        return false;
    }

    auto& state = it->second;

    const auto findPosition = [&](const std::string& nodeId) {
        if (const auto posIt = state.layout.find(nodeId); posIt != state.layout.end()) {
            return posIt->second;
        }
        return persistence::LayoutPosition { 0.5F, 0.5F };
    };

    const auto firstPos = findPosition(first);
    const auto secondPos = findPosition(second);

    state.layout[first] = secondPos;
    state.layout[second] = firstPos;

    saveProject();
    log(LogCategory::Ui, "swapMicroNodes {} <-> {} in {}", first, second, viewId);
    return true;
}

bool Application::insertNodeIntoConnection(const std::string& nodeId, const std::pair<std::string, std::string>& connection) {
    if (!currentProject_.graphTopology) {
        return false;
    }

    if (nodeId == connection.first || nodeId == connection.second) {
        return false;
    }

    const auto node = currentProject_.graphTopology->findNode(nodeId);
    if (!node) {
        return false;
    }

    if (!currentProject_.graphTopology->findNode(connection.first) || !currentProject_.graphTopology->findNode(connection.second)) {
        return false;
    }

    const auto channelsIn = node->inputChannelCount();
    const auto channelsOut = node->outputChannelCount();
    std::vector<audio::GraphConnection> previous;
    detachNodeConnections(*currentProject_.graphTopology, nodeId, previous);

    if (!rewireForInsertion(*currentProject_.graphTopology,
            std::optional<std::pair<std::string, std::string>>(connection),
            nodeId,
            channelsIn,
            channelsOut)) {
        restoreConnections(*currentProject_.graphTopology, previous);
        return false;
    }

    applyAudioTopology();
    nodeGraphView_.setTopology(currentProject_.graphTopology);
    saveProject();
    log(LogCategory::Ui, "insertNodeIntoConnection {} between {} -> {}", nodeId, connection.first, connection.second);
    return true;
}

bool Application::insertMicroNodeIntoConnection(const std::string& viewId,
                                                const std::string& nodeId,
                                                const std::pair<std::string, std::string>& connection) {
    ensureMicroView(viewId);
    auto it = currentProject_.microViews.find(viewId);
    if (it == currentProject_.microViews.end() || !it->second.topology) {
        return false;
    }

    auto& state = it->second;
    if (nodeId == connection.first || nodeId == connection.second) {
        return false;
    }

    const auto node = state.topology->findNode(nodeId);
    if (!node) {
        return false;
    }

    if (!state.topology->findNode(connection.first) || !state.topology->findNode(connection.second)) {
        return false;
    }

    const auto channelsIn = node->inputChannelCount();
    const auto channelsOut = node->outputChannelCount();
    std::vector<audio::GraphConnection> previous;
    detachNodeConnections(*state.topology, nodeId, previous);

    if (!rewireForInsertion(*state.topology,
            std::optional<std::pair<std::string, std::string>>(connection),
            nodeId,
            channelsIn,
            channelsOut)) {
        restoreConnections(*state.topology, previous);
        return false;
    }

    applyAudioTopology();
    saveProject();
    std::cout << "[Application] insertMicroNodeIntoConnection node=" << nodeId
              << " between " << connection.first << " -> " << connection.second
              << " view=" << viewId << std::endl;
    return true;
}

bool Application::configureNodeChannels(const std::string& nodeId,
                                        std::uint32_t inputChannels,
                                        std::uint32_t outputChannels) {
    inputChannels = std::min<std::uint32_t>(inputChannels, 2U);
    outputChannels = std::min<std::uint32_t>(outputChannels, 2U);

    if (!currentProject_.graphTopology) {
        return false;
    }

    const auto nodeCopy = currentProject_.graphTopology->findNode(nodeId);
    if (!nodeCopy) {
        return false;
    }

    if (!currentProject_.graphTopology->setNodeChannelCounts(nodeId, inputChannels, outputChannels)) {
        return false;
    }

    updateMicroTopologyForNode(nodeId);

    applyAudioTopology();
    nodeGraphView_.setTopology(currentProject_.graphTopology);
    saveProject();
    return true;
}

} // namespace broadcastmix::core
