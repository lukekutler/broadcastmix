#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <ui/NodeGraphView.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <utility>
#include <array>

namespace broadcastmix::app {

class NodeGraphComponent : public juce::Component, public juce::DragAndDropTarget, private juce::Timer {
public:
    struct NodeCreateRequest {
        std::string templateId;
        float normX { 0.5F };
        float normY { 0.5F };
        std::optional<std::pair<std::string, std::string>> insertBetween;
    };

    explicit NodeGraphComponent(ui::NodeGraphView* view);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setNodeDoubleClickHandler(std::function<void(const std::string&)> handler);
    void setNodeDragHandler(std::function<void(const std::string&, float, float)> handler);
    void setMeterProvider(std::function<std::array<float, 2>(const std::string&)> provider);
    void setGraphView(ui::NodeGraphView* view);
    void setConnectNodesHandler(std::function<void(const std::string&, const std::string&)> handler);
    void setDisconnectNodesHandler(std::function<void(const std::string&, const std::string&)> handler);
    void setSelectionChangedHandler(std::function<void(const std::optional<std::string>&)> handler);
    void setPortConnectHandler(std::function<void(const std::string&, std::size_t, const std::string&, std::size_t)> handler);
    void setNodeCreateHandler(std::function<void(const NodeCreateRequest&)> handler);
    void setNodeSwapHandler(std::function<void(const std::string&, const std::string&)> handler);
    void setNodeInsertHandler(std::function<void(const std::string&, const std::pair<std::string, std::string>&)> handler);
    void setFixedEndpoints(std::optional<std::string> inputId, std::optional<std::string> outputId);
    [[nodiscard]] std::optional<std::string> selectedNode() const noexcept;

private:
    struct PortSelection {
        std::string nodeId;
        bool isOutput;
        std::size_t index;
        bool operator==(const PortSelection&) const = default;
    };

    struct ConnectionSegment {
        std::string fromId;
        std::string toId;
        juce::Line<float> line;
    };

    void timerCallback() override;
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;
    bool shouldDrawDragImageWhenOver() override { return false; }
    void refreshDropTargets();
    [[nodiscard]] std::optional<std::pair<std::string, std::string>> connectionNear(const juce::Point<float>& position) const;
    void resolveFixedEndpoints();
    [[nodiscard]] juce::Colour toColour(const ui::Color& color) const;
    [[nodiscard]] juce::Colour nodeFillColour(audio::GraphNodeType type) const;
    void refreshCachedPositions();
    [[nodiscard]] juce::Rectangle<float> computeLayoutArea() const;
    [[nodiscard]] juce::Rectangle<float> nodeBoundsForPosition(const juce::Point<float>& position) const;
    [[nodiscard]] std::optional<std::string> hitTestNode(const juce::Point<float>& position) const;
    [[nodiscard]] std::optional<PortSelection> findPortAt(const juce::Point<float>& position) const;
    [[nodiscard]] juce::Point<float> portPosition(const PortSelection& port) const;

    ui::NodeGraphView* view_ { nullptr };
    std::size_t lastLayoutVersion_ { 0 };
    std::unordered_map<std::string, juce::Point<float>> cachedPositions_;
    std::optional<std::string> draggingNodeId_;
    std::optional<std::string> selectedNodeId_;
    juce::Point<float> dragOffset_ {};
    juce::Rectangle<float> layoutArea_ {};
    std::function<void(const std::string&)> onNodeDoubleClicked_;
    std::function<void(const std::string&, float, float)> onNodeDragged_;
    std::function<std::array<float, 2>(const std::string&)> meterProvider_;
    std::function<void(const std::string&, const std::string&)> onConnectNodes_;
    std::function<void(const std::string&, const std::string&)> onDisconnectNodes_;
    std::function<void(const std::optional<std::string>&)> onSelectionChanged_;
    std::function<void(const std::string&, std::size_t, const std::string&, std::size_t)> onPortConnected_;
    std::function<void(const NodeCreateRequest&)> onNodeCreated_;
    std::function<void(const std::string&, const std::string&)> onNodesSwapped_;
    std::function<void(const std::string&, const std::pair<std::string, std::string>&)> onNodeInserted_;

    std::unordered_map<std::string, std::vector<juce::Point<float>>> inputPortPositions_;
    std::unordered_map<std::string, std::vector<juce::Point<float>>> outputPortPositions_;
    std::optional<PortSelection> draggingPort_;
    std::optional<PortSelection> hoverPort_;
    juce::Point<float> dragPosition_ {};
    std::vector<ConnectionSegment> connectionSegments_;
    std::optional<std::pair<std::string, std::string>> selectedConnection_;
    std::optional<juce::Point<float>> pendingDropPosition_;
    std::optional<std::string> pendingDropType_;
    std::optional<std::pair<std::string, std::string>> pendingDropConnection_;
    std::optional<std::string> swapTargetId_;
    std::optional<std::string> fixedInputId_;
    std::optional<std::string> fixedOutputId_;
    std::optional<juce::Point<float>> fixedInputAnchor_;
    std::optional<juce::Point<float>> fixedOutputAnchor_;
    std::optional<float> fixedInputNormY_;
    std::optional<float> fixedOutputNormY_;
    bool fixedInputEnabled_ { false };
    bool fixedOutputEnabled_ { false };
};

} // namespace broadcastmix::app
