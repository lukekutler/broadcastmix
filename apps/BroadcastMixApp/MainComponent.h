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

namespace broadcastmix::audio {
class GraphNode;
}

namespace broadcastmix::persistence {
struct LayoutPosition;
}

namespace broadcastmix::app {

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::ComboBox::Listener,
                      private juce::TextEditor::Listener {
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
    void navigateToBreadcrumbIndex(int index);
    ui::NodeGraphView::PositionOverrideMap buildOverrides(const std::unordered_map<std::string, persistence::LayoutPosition>& layout) const;
    [[nodiscard]] std::string labelForNode(const std::string& nodeId) const;
    [[nodiscard]] std::optional<core::Application::NodeTemplate> templateForLibraryId(const std::string& id) const;
    [[nodiscard]] bool isChannelNode(const std::string& nodeId) const;
    void refreshSetupPanel();
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorFocusLost(juce::TextEditor& editor) override;
    void refreshBreadcrumbBar();
    void handleRenameSuccess(const std::string& nodeId);
    void applyPersonUpdate();
    void applyRoleUpdate();
    void chooseProfileImage();
    void clearProfileImage();
    void updateAvatarDisplay(const audio::GraphNode& node);
    void saveCurrentPositionPreset();

    class AvatarComponent : public juce::Component {
    public:
        void setTheme(juce::Colour fill, juce::Colour outline, juce::Colour text);
        void setImage(const juce::Image& image);
        void clearImage();
        void setInitials(juce::String initials);
        void paint(juce::Graphics& g) override;

    private:
        juce::Image image_;
        juce::String initials_;
        juce::Colour fillColour_ { juce::Colours::darkgrey };
        juce::Colour outlineColour_ { juce::Colours::black };
        juce::Colour textColour_ { juce::Colours::white };
    };

    core::Application& app_;
    NodeGraphComponent graphComponent_;
    NodeLibraryComponent nodeLibrary_;
    juce::Label headline_;
    juce::Label subtext_;
    juce::Label breadcrumbs_;
    juce::TextButton backButton_ { "Back" };
    juce::Component breadcrumbBar_;
    juce::OwnedArray<juce::TextButton> breadcrumbButtons_;
    juce::GroupComponent setupGroup_ { "setupGroup", "Setup" };
    juce::Label inputLabel_ { "inputLabel", "Input" };
    juce::ComboBox inputFormatBox_;
    juce::Label outputLabel_ { "outputLabel", "Output" };
    juce::ComboBox outputFormatBox_;
    juce::Label personLabel_ { "personLabel", "Person" };
    juce::TextEditor personEditor_;
    juce::Label roleLabel_ { "roleLabel", "Role" };
    juce::TextEditor roleEditor_;
    juce::Label presetLabel_ { "presetLabel", "Preset" };
    juce::ComboBox presetBox_;
    juce::TextButton savePresetButton_ { "Save Preset..." };
    juce::Label profileLabel_ { "profileLabel", "Setup" };
    AvatarComponent avatarPreview_;
    juce::TextButton chooseImageButton_ { "Select Image..." };
    juce::TextButton clearImageButton_ { "Clear" };
    std::optional<MicroViewContext> currentMicro_;
    std::optional<std::string> selectedNode_;
    bool suppressSetupEvents_ { false };
    std::string currentProfileImagePath_;
    std::unique_ptr<juce::FileChooser> activeFileChooser_;
    std::vector<std::pair<std::string, std::string>> breadcrumbStack_;
};

} // namespace broadcastmix::app
