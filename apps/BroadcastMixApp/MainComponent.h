#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <core/Application.h>

#include "NodeGraphComponent.h"
#include "NodeLibraryComponent.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace broadcastmix::core {
class Application;
}

namespace broadcastmix::persistence {
struct LayoutPosition;
}

namespace broadcastmix::app {

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::ComboBox::Listener {
public:
    explicit MainComponent(core::Application& app);

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    struct MicroViewContext {
        std::string id;
        std::string label;
        std::unique_ptr<ui::NodeGraphView> view;
    };

    juce::Colour toColour(const ui::Color& color) const;
    void handleNodeDoubleClick(const std::string& nodeId);
    void switchToMacroView();
    void switchToMicroView(const std::string& nodeId, const std::string& label, const core::Application::MicroViewDescriptor& descriptor);
    void updateBreadcrumbs();
    void navigateBack();
    ui::NodeGraphView::PositionOverrideMap buildOverrides(const std::unordered_map<std::string, persistence::LayoutPosition>& layout) const;
    [[nodiscard]] std::string labelForNode(const std::string& nodeId) const;
    [[nodiscard]] std::optional<core::Application::NodeTemplate> templateForLibraryId(const std::string& id) const;
    [[nodiscard]] bool isChannelNode(const std::string& nodeId) const;
    void refreshIoConfigPanel();
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    core::Application& app_;
    NodeGraphComponent graphComponent_;
    NodeLibraryComponent nodeLibrary_;
    juce::Label headline_;
    juce::Label subtext_;
    juce::Label breadcrumbs_;
    juce::TextButton backButton_ { "Back" };
    juce::GroupComponent ioGroup_ { "ioGroup", "I/O Configuration" };
    juce::Label inputLabel_ { "inputLabel", "Input" };
    juce::ComboBox inputFormatBox_;
    juce::Label outputLabel_ { "outputLabel", "Output" };
    juce::ComboBox outputFormatBox_;
    std::optional<MicroViewContext> currentMicro_;
    std::optional<std::string> selectedNode_;
    bool suppressIoEvents_ { false };
};

} // namespace broadcastmix::app
