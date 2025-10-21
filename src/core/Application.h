#pragma once

#include "../audio/AudioEngine.h"
#include "../control/ControlSurfaceManager.h"
#include "../persistence/ProjectSerializer.h"
#include "../plugins/PluginHost.h"
#include "../ui/NodeGraphView.h"
#include "../update/UpdateService.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <cstddef>
#include <utility>
#include <array>

namespace broadcastmix::core {

struct ApplicationConfig {
    std::string appName { "BroadcastMix" };
    std::string version { "3.0.0" };
};

class Application {
public:
    enum class NodeTemplate {
        Channel,
        Output,
        Group,
        Effect,
        SignalGenerator
    };

    struct MicroViewDescriptor {
        std::shared_ptr<audio::GraphTopology> topology;
        std::unordered_map<std::string, persistence::LayoutPosition> layout;
    };

    Application(ApplicationConfig config,
                audio::AudioEngineSettings audioSettings);

    void initialize();
    void loadProject(const std::string& path);
    void run();
    void startRealtimeEngine();
    void stopRealtimeEngine();
    [[nodiscard]] audio::AudioEngineStatus audioStatus() const;
    [[nodiscard]] std::optional<audio::GraphNodeType> nodeTypeForId(const std::string& nodeId) const;
    [[nodiscard]] std::optional<audio::GraphNode> nodeForId(const std::string& nodeId) const;
    [[nodiscard]] std::shared_ptr<const audio::GraphTopology> graphTopology() const;
    [[nodiscard]] audio::AudioEngineSettings audioSettings() const;
    [[nodiscard]] ui::NodeGraphView& nodeGraphView() noexcept;
    [[nodiscard]] const ui::NodeGraphView& nodeGraphView() const noexcept;
    [[nodiscard]] MicroViewDescriptor microViewDescriptor(const std::string& viewId);
    void updateMacroNodePosition(const std::string& nodeId, float normX, float normY);
    void updateMicroNodePosition(const std::string& viewId, const std::string& nodeId, float normX, float normY);
    [[nodiscard]] std::array<float, 2> meterLevelForNode(const std::string& nodeId) const;
    [[nodiscard]] std::array<float, 2> meterLevelForMicroNode(const std::string& viewId, const std::string& nodeId) const;
    [[nodiscard]] const std::unordered_map<std::string, persistence::LayoutPosition>& macroLayout() const noexcept;
    bool deleteNode(const std::string& nodeId);
    bool toggleNodeEnabled(const std::string& nodeId);
    bool connectNodes(const std::string& fromId, const std::string& toId);
    bool disconnectNodes(const std::string& fromId, const std::string& toId);
    bool connectNodePorts(const std::string& fromId, std::size_t fromChannel, const std::string& toId, std::size_t toChannel);
    bool deleteMicroNode(const std::string& viewId, const std::string& nodeId);
    bool toggleMicroNodeEnabled(const std::string& viewId, const std::string& nodeId);
    bool connectMicroNodes(const std::string& viewId, const std::string& fromId, const std::string& toId);
    bool disconnectMicroNodes(const std::string& viewId, const std::string& fromId, const std::string& toId);
    bool connectMicroNodePorts(const std::string& viewId,
                               const std::string& fromId,
                               std::size_t fromChannel,
                               const std::string& toId,
                               std::size_t toChannel);
    bool createNode(NodeTemplate type, float normX, float normY, std::optional<std::pair<std::string, std::string>> insertBetween = std::nullopt);
    bool createMicroNode(const std::string& viewId,
                         NodeTemplate type,
                         float normX,
                         float normY,
                         std::optional<std::pair<std::string, std::string>> insertBetween = std::nullopt);
    bool swapMacroNodes(const std::string& first, const std::string& second);
    bool swapMicroNodes(const std::string& viewId, const std::string& first, const std::string& second);
    bool insertNodeIntoConnection(const std::string& nodeId, const std::pair<std::string, std::string>& connection);
    bool insertMicroNodeIntoConnection(const std::string& viewId,
                                       const std::string& nodeId,
                                       const std::pair<std::string, std::string>& connection);
    bool configureNodeChannels(const std::string& nodeId,
                               std::uint32_t inputChannels,
                               std::uint32_t outputChannels);
    bool renameNode(const std::string& nodeId, const std::string& newLabel);

private:
    void applyMacroLayout();
    void saveProject();
    MicroViewDescriptor ensureMicroView(const std::string& viewId);
    std::string templatePrefix(NodeTemplate type) const;
    audio::GraphNodeType graphTypeForTemplate(NodeTemplate type) const;
    [[nodiscard]] std::optional<NodeTemplate> templateForGraphType(audio::GraphNodeType type) const;
    void configureChannelsForTemplate(audio::GraphNode& node, NodeTemplate type) const;
    [[nodiscard]] std::string labelBase(NodeTemplate type) const;
    std::string makeLabel(NodeTemplate type, std::size_t index) const;
    std::string nextNodeId(NodeTemplate type);
    std::string nextMicroNodeId(const std::string& viewId, NodeTemplate type, audio::GraphTopology& topology);
    void renumberMacroNodes(NodeTemplate type);
    void renumberMicroNodes(const std::string& viewId);
    [[nodiscard]] std::uint32_t channelCountForMicroInsertion(const audio::GraphTopology& topology,
                                                              const std::optional<std::pair<std::string, std::string>>& insertBetween) const;
    void updateMicroTopologyForNode(const std::string& nodeId);
    [[nodiscard]] audio::GraphNodeType resolveNodeType(const std::string& nodeId) const;
    static bool rewireForInsertion(audio::GraphTopology& topology,
                                   const std::optional<std::pair<std::string, std::string>>& insertBetween,
                                   const std::string& newNodeId,
                                   std::uint32_t newInputChannels,
                                   std::uint32_t newOutputChannels);
    static void detachNodeConnections(audio::GraphTopology& topology,
                                      const std::string& nodeId,
                                      std::vector<audio::GraphConnection>& removedConnections);
    static void restoreConnections(audio::GraphTopology& topology,
                                   const std::vector<audio::GraphConnection>& connections);
    [[nodiscard]] std::shared_ptr<audio::GraphTopology> buildAudioTopology() const;
    void applyAudioTopology();

    ApplicationConfig config_;
    audio::AudioEngine audioEngine_;
    plugins::PluginHost pluginHost_;
    persistence::ProjectSerializer projectSerializer_;
    control::ControlSurfaceManager controlManager_;
    ui::NodeGraphView nodeGraphView_;
    update::UpdateService updateService_;
    persistence::Project currentProject_ {};
    std::optional<std::string> currentProjectPath_;
    bool projectLoaded_ { false };
    std::unordered_map<std::string, std::size_t> nodeCounters_;
    std::unordered_map<std::string, std::size_t> microNodeCounters_;
    mutable std::unordered_map<std::string, std::string> meterAliases_;
};

} // namespace broadcastmix::core
