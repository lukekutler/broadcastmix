#include "MainComponent.h"

#include <core/Application.h>
#include <persistence/ProjectSerializer.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>

namespace broadcastmix::app {

namespace {
constexpr auto kHeadlineText = "BroadcastMix v3";
constexpr auto kSubText = "Drag nodes from the library to build your mix.";
} // namespace

MainComponent::MainComponent(core::Application& app)
    : app_(app)
    , graphComponent_(&app.nodeGraphView()) {
    setOpaque(true);

    graphComponent_.setNodeDoubleClickHandler([this](const std::string& nodeId) {
        handleNodeDoubleClick(nodeId);
    });
    graphComponent_.setSelectionChangedHandler([this](const std::optional<std::string>& nodeId) {
        selectedNode_ = nodeId;
        refreshIoConfigPanel();
    });

    headline_.setText(kHeadlineText, juce::dontSendNotification);
    headline_.setJustificationType(juce::Justification::centred);
    headline_.setFont(juce::Font(juce::FontOptions { 28.0F, juce::Font::bold }));

    subtext_.setText(kSubText, juce::dontSendNotification);
    subtext_.setJustificationType(juce::Justification::centred);
    subtext_.setFont(juce::Font(juce::FontOptions { 15.0F }));

    breadcrumbs_.setJustificationType(juce::Justification::centredLeft);
    breadcrumbs_.setFont(juce::Font(juce::FontOptions { 14.0F }));

    backButton_.onClick = [this]() {
        navigateBack();
    };
    backButton_.setVisible(false);
    breadcrumbs_.setVisible(false);
    breadcrumbBar_.setInterceptsMouseClicks(true, true);

    addAndMakeVisible(headline_);
    addAndMakeVisible(subtext_);
    addAndMakeVisible(breadcrumbs_);
    addAndMakeVisible(backButton_);
    addAndMakeVisible(nodeLibrary_);
    addAndMakeVisible(graphComponent_);
    addAndMakeVisible(ioGroup_);
    addAndMakeVisible(inputLabel_);
    addAndMakeVisible(inputFormatBox_);
    addAndMakeVisible(outputLabel_);
    addAndMakeVisible(outputFormatBox_);
    addAndMakeVisible(breadcrumbBar_);

    ioGroup_.setVisible(false);
    inputLabel_.setVisible(false);
    inputFormatBox_.setVisible(false);
    outputLabel_.setVisible(false);
    outputFormatBox_.setVisible(false);

    inputLabel_.setJustificationType(juce::Justification::centredLeft);
    outputLabel_.setJustificationType(juce::Justification::centredLeft);
    inputFormatBox_.addListener(this);
    outputFormatBox_.addListener(this);
    inputFormatBox_.setJustificationType(juce::Justification::centredLeft);
    outputFormatBox_.setJustificationType(juce::Justification::centredLeft);

    nodeLibrary_.setTheme(app_.nodeGraphView().theme());

    switchToMacroView();
    refreshIoConfigPanel();
}

void MainComponent::paint(juce::Graphics& g) {
    const auto& theme = app_.nodeGraphView().theme();
    g.fillAll(toColour(theme.background));

    auto bounds = getLocalBounds().toFloat().reduced(16.0F);
    const auto highlightTop = toColour(theme.accent).withAlpha(0.25F);
    const auto highlightBottom = toColour(theme.background).withAlpha(0.0F);
    g.setGradientFill(juce::ColourGradient::vertical(
        highlightTop,
        bounds.getY(),
        highlightBottom,
        bounds.getBottom()));
    g.fillRoundedRectangle(bounds, 18.0F);
}

void MainComponent::resized() {
    auto area = getLocalBounds().reduced(24);
    auto headerArea = area.removeFromTop(132);
    auto headlineArea = headerArea.removeFromTop(60);
    headline_.setBounds(headlineArea);
    headerArea.removeFromTop(6);
    subtext_.setBounds(headerArea.removeFromTop(24));
    headerArea.removeFromTop(6);

    auto breadcrumbArea = headerArea.removeFromTop(28);
    breadcrumbBar_.setBounds(breadcrumbArea);
    refreshBreadcrumbBar();

    area.removeFromTop(12);
    const int idealLibraryWidth = juce::jlimit(200, 260, area.getWidth() / 3);
    const int libraryWidth = std::min(area.getWidth(), idealLibraryWidth);
    auto libraryArea = area.removeFromLeft(libraryWidth);
    const int ioHeight = ioGroup_.isVisible() ? 140 : 0;
    juce::Rectangle<int> configArea;
    if (ioHeight > 0) {
        configArea = libraryArea.removeFromBottom(ioHeight);
        ioGroup_.setBounds(configArea);
        auto content = configArea.reduced(12);
        if (inputFormatBox_.isVisible()) {
            auto inputRow = content.removeFromTop(28);
            inputLabel_.setBounds(inputRow.removeFromLeft(70));
            inputFormatBox_.setBounds(inputRow);
            content.removeFromTop(8);
        } else {
            inputLabel_.setBounds({});
            inputFormatBox_.setBounds({});
        }

        if (outputFormatBox_.isVisible()) {
            auto outputRow = content.removeFromTop(28);
            outputLabel_.setBounds(outputRow.removeFromLeft(70));
            outputFormatBox_.setBounds(outputRow);
        } else {
            outputLabel_.setBounds({});
            outputFormatBox_.setBounds({});
        }
    } else {
        ioGroup_.setBounds({});
        inputLabel_.setBounds({});
        inputFormatBox_.setBounds({});
        outputLabel_.setBounds({});
        outputFormatBox_.setBounds({});
    }

    nodeLibrary_.setBounds(libraryArea);
    area.removeFromLeft(16);
    graphComponent_.setBounds(area);
}

juce::Colour MainComponent::toColour(const ui::Color& color) const {
    return juce::Colour::fromFloatRGBA(color.r, color.g, color.b, color.a);
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    juce::Logger::writeToLog("MainComponent :: keyPressed code=" + juce::String(key.getKeyCode())
        + " cmd=" + juce::String(key.getModifiers().isCommandDown() ? "true" : "false")
        + " macro=" + juce::String(!currentMicro_.has_value() ? "true" : "false")
        + " selection=" + (selectedNode_ ? *selectedNode_ : juce::String("<none>")));

    if (graphComponent_.keyPressed(key)) {
        return true;
    }

    if (!selectedNode_) {
        return Component::keyPressed(key);
    }

    const bool isMacroView = !currentMicro_.has_value();

    const auto handleDelete = [&]() {
        bool changed = false;
        if (isMacroView) {
            changed = app_.deleteNode(*selectedNode_);
            if (changed) {
                switchToMacroView();
            }
        } else {
            changed = app_.deleteMicroNode(currentMicro_->id, *selectedNode_);
            if (changed) {
                auto descriptor = app_.microViewDescriptor(currentMicro_->id);
                switchToMicroView(currentMicro_->id, labelForNode(currentMicro_->id), descriptor);
            }
        }
        if (changed) {
            selectedNode_.reset();
        }
        return changed;
    };

    const auto handleToggle = [&]() {
        bool changed = false;
        if (isMacroView) {
            changed = app_.toggleNodeEnabled(*selectedNode_);
            if (changed) {
                switchToMacroView();
            }
        } else {
            changed = app_.toggleMicroNodeEnabled(currentMicro_->id, *selectedNode_);
            if (changed) {
                auto descriptor = app_.microViewDescriptor(currentMicro_->id);
                switchToMicroView(currentMicro_->id, labelForNode(currentMicro_->id), descriptor);
            }
        }
        return changed;
    };

    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey) {
        if (handleDelete()) {
            juce::Logger::writeToLog("MainComponent :: delete handled");
            return true;
        }
    }

    if ((key.getKeyCode() == 'd' || key.getKeyCode() == 'D') && key.getModifiers().isCommandDown()) {
        if (handleToggle()) {
            juce::Logger::writeToLog("MainComponent :: toggle handled");
            return true;
        }
    }

    return Component::keyPressed(key);
}

void MainComponent::handleNodeDoubleClick(const std::string& nodeId) {
    std::cout << "[MainComponent] handleNodeDoubleClick id=" << nodeId << std::endl;
    const auto descriptor = app_.microViewDescriptor(nodeId);
    if (!descriptor.topology) {
        return;
    }

    auto label = labelForNode(nodeId);

    switchToMicroView(nodeId, label, descriptor);
}

void MainComponent::switchToMicroView(const std::string& nodeId,
                                      const std::string& label,
                                      const core::Application::MicroViewDescriptor& descriptor) {
    if (!descriptor.topology) {
        return;
    }

    std::string effectiveNodeId = nodeId;
    if (effectiveNodeId.empty() && currentMicro_ && !currentMicro_->id.empty()) {
        effectiveNodeId = currentMicro_->id;
    }
    if (effectiveNodeId.empty()) {
        std::cout << "[MainComponent] switchToMicroView aborted: empty node id" << std::endl;
        return;
    }

    std::string effectiveLabel = label;
    const std::string parentId = currentMicro_ ? currentMicro_->id : std::string{};

    if (currentMicro_ && currentMicro_->id == effectiveNodeId) {
        currentMicro_->label = effectiveLabel;
        currentMicro_->view->setPositionOverrides(buildOverrides(descriptor.layout));
        currentMicro_->view->setTopology(descriptor.topology);
    } else {
        currentMicro_ = MicroViewContext {
            .id = effectiveNodeId,
            .label = effectiveLabel,
            .view = std::make_unique<ui::NodeGraphView>()
        };
        currentMicro_->view->loadTheme(app_.nodeGraphView().theme());
        currentMicro_->view->setPositionOverrides(buildOverrides(descriptor.layout));
        currentMicro_->view->setTopology(descriptor.topology);
    }

    if (parentId.empty()) {
        breadcrumbStack_.clear();
    }

    breadcrumbStack_.emplace_back(effectiveNodeId, effectiveLabel);

    graphComponent_.setGraphView(currentMicro_->view.get());
    graphComponent_.setNodeDragHandler([this, effectiveNodeId](const std::string& childNode, float normX, float normY) {
        app_.updateMicroNodePosition(effectiveNodeId, childNode, normX, normY);
    });
    graphComponent_.setMeterProvider([this, effectiveNodeId](const std::string& childNode) {
        return app_.meterLevelForMicroNode(effectiveNodeId, childNode);
    });
    graphComponent_.setConnectNodesHandler([this, effectiveNodeId](const std::string& fromId, const std::string& toId) {
        std::cout << "[MainComponent] connect handler node=" << effectiveNodeId << " from=" << fromId << " to=" << toId << std::endl;
        if (app_.connectMicroNodes(effectiveNodeId, fromId, toId)) {
            auto descriptor = app_.microViewDescriptor(effectiveNodeId);
            switchToMicroView(effectiveNodeId, labelForNode(effectiveNodeId), descriptor);
        }
    });
    graphComponent_.setDisconnectNodesHandler([this, effectiveNodeId](const std::string& fromId, const std::string& toId) {
        std::cout << "[MainComponent] disconnect handler node=" << effectiveNodeId << " from=" << fromId << " to=" << toId << std::endl;
        if (app_.disconnectMicroNodes(effectiveNodeId, fromId, toId)) {
            auto descriptor = app_.microViewDescriptor(effectiveNodeId);
            switchToMicroView(effectiveNodeId, labelForNode(effectiveNodeId), descriptor);
        }
    });
    graphComponent_.setPortConnectHandler([this, effectiveNodeId](const std::string& fromId, std::size_t, const std::string& toId, std::size_t) {
        std::cout << "[MainComponent] port connect handler node=" << effectiveNodeId << " from=" << fromId << " to=" << toId << std::endl;
        if (app_.connectMicroNodes(effectiveNodeId, fromId, toId)) {
            auto descriptor = app_.microViewDescriptor(effectiveNodeId);
            switchToMicroView(effectiveNodeId, labelForNode(effectiveNodeId), descriptor);
        }
    });
    graphComponent_.setNodeCreateHandler([this, effectiveNodeId](const NodeGraphComponent::NodeCreateRequest& request) {
        std::cout << "[MainComponent] create handler node=" << effectiveNodeId
                  << " template=" << request.templateId
                  << " insertBetween="
                  << (request.insertBetween ? (request.insertBetween->first + "->" + request.insertBetween->second) : std::string("<none>"))
                  << std::endl;
        const auto templateType = templateForLibraryId(request.templateId);
        if (!templateType) {
            return;
        }

        if (app_.createMicroNode(effectiveNodeId, *templateType, request.normX, request.normY, request.insertBetween)) {
            auto descriptor = app_.microViewDescriptor(effectiveNodeId);
            std::cout << "[MainComponent] create handler invoking switchToMicroView id=" << effectiveNodeId << std::endl;
            switchToMicroView(effectiveNodeId, labelForNode(effectiveNodeId), descriptor);
        }
    });
    graphComponent_.setNodeSwapHandler([this, effectiveNodeId](const std::string& first, const std::string& second) {
        std::cout << "[MainComponent] swap handler node=" << effectiveNodeId << " first=" << first << " second=" << second << std::endl;
        if (app_.swapMicroNodes(effectiveNodeId, first, second)) {
            auto descriptor = app_.microViewDescriptor(effectiveNodeId);
            switchToMicroView(effectiveNodeId, labelForNode(effectiveNodeId), descriptor);
        }
    });
    graphComponent_.setNodeInsertHandler([this, effectiveNodeId](const std::string& insertNode, const std::pair<std::string, std::string>& connection) {
        std::cout << "[MainComponent] insert handler node=" << effectiveNodeId
                  << " insertNode=" << insertNode
                  << " between=" << connection.first << " -> " << connection.second << std::endl;
        if (app_.insertMicroNodeIntoConnection(effectiveNodeId, insertNode, connection)) {
            auto descriptor = app_.microViewDescriptor(effectiveNodeId);
            std::cout << "[MainComponent] insert handler invoking switchToMicroView id=" << effectiveNodeId << std::endl;
            switchToMicroView(effectiveNodeId, labelForNode(effectiveNodeId), descriptor);
        }
    });

    const auto channelInputId = effectiveNodeId + "_input";
    const auto channelOutputId = effectiveNodeId + "_output";
    const bool inputExists = descriptor.topology && descriptor.topology->findNode(channelInputId).has_value();
    const bool outputExists = descriptor.topology && descriptor.topology->findNode(channelOutputId).has_value();

    audio::GraphNodeType macroType = audio::GraphNodeType::Utility;
    if (const auto nodeType = app_.nodeTypeForId(effectiveNodeId)) {
        macroType = *nodeType;
    }

    std::optional<std::string> fixedInput;
    std::optional<std::string> fixedOutput;

    if ((macroType == audio::GraphNodeType::Channel || macroType == audio::GraphNodeType::Output) && inputExists) {
        fixedInput = channelInputId;
    }
    if ((macroType == audio::GraphNodeType::Channel || macroType == audio::GraphNodeType::GroupBus || macroType == audio::GraphNodeType::Output) && outputExists) {
        fixedOutput = channelOutputId;
    }

    const auto resolvedLabel = labelForNode(effectiveNodeId);
    currentMicro_->label = resolvedLabel;

    if (parentId.empty()) {
        breadcrumbStack_.clear();
    } else {
        const auto parentIt = std::find_if(breadcrumbStack_.begin(), breadcrumbStack_.end(), [&](const auto& entry) {
            return entry.first == parentId;
        });
        if (parentIt != breadcrumbStack_.end()) {
            breadcrumbStack_.erase(std::next(parentIt), breadcrumbStack_.end());
        } else {
            breadcrumbStack_.clear();
            breadcrumbStack_.emplace_back(parentId, labelForNode(parentId));
        }
    }

    const auto existing = std::find_if(breadcrumbStack_.begin(), breadcrumbStack_.end(), [&](const auto& entry) {
        return entry.first == effectiveNodeId;
    });

    if (existing == breadcrumbStack_.end()) {
        breadcrumbStack_.emplace_back(effectiveNodeId, resolvedLabel);
    } else {
        existing->second = resolvedLabel;
        breadcrumbStack_.erase(std::next(existing), breadcrumbStack_.end());
    }

    effectiveLabel = resolvedLabel;

    std::cout << "[MainComponent] switchToMicroView node='";
    for (char c : effectiveNodeId) {
        std::cout << c;
    }
    std::cout << "' len=" << effectiveNodeId.size()
              << " inputExists=" << (inputExists ? "true" : "false")
              << " outputExists=" << (outputExists ? "true" : "false")
              << " fixedInput=" << (fixedInput ? fixedInput->c_str() : "<none>")
              << " fixedOutput=" << (fixedOutput ? fixedOutput->c_str() : "<none>") << std::endl;
    if (!effectiveNodeId.empty()) {
        std::cout << "    nodeId ASCII:";
        for (unsigned char c : effectiveNodeId) {
            std::cout << ' ' << static_cast<int>(c);
        }
        std::cout << std::endl;
    }
    for (const auto& node : descriptor.topology->nodes()) {
        std::cout << "    micro node id=" << node.id()
                  << " type=" << static_cast<int>(node.type()) << std::endl;
    }
    graphComponent_.setFixedEndpoints(fixedInput, fixedOutput);

    updateBreadcrumbs();
    selectedNode_.reset();
    graphComponent_.grabKeyboardFocus();
    refreshIoConfigPanel();
}

void MainComponent::switchToMacroView() {
    currentMicro_.reset();
    std::cout << "[MainComponent] switchToMacroView" << std::endl;
    breadcrumbStack_.clear();
    graphComponent_.setGraphView(&app_.nodeGraphView());
    graphComponent_.setNodeDragHandler([this](const std::string& nodeId, float normX, float normY) {
        app_.updateMacroNodePosition(nodeId, normX, normY);
    });
    graphComponent_.setMeterProvider([this](const std::string& nodeId) {
        return app_.meterLevelForNode(nodeId);
    });
    graphComponent_.setConnectNodesHandler([this](const std::string& fromId, const std::string& toId) {
        if (app_.connectNodes(fromId, toId)) {
            switchToMacroView();
        }
    });
    graphComponent_.setDisconnectNodesHandler([this](const std::string& fromId, const std::string& toId) {
        if (app_.disconnectNodes(fromId, toId)) {
            switchToMacroView();
        }
    });
    graphComponent_.setPortConnectHandler([this](const std::string& fromId, std::size_t, const std::string& toId, std::size_t) {
        if (app_.connectNodes(fromId, toId)) {
            switchToMacroView();
        }
    });
    graphComponent_.setNodeCreateHandler([this](const NodeGraphComponent::NodeCreateRequest& request) {
        const auto templateType = templateForLibraryId(request.templateId);
        if (!templateType) {
            return;
        }

        if (app_.createNode(*templateType, request.normX, request.normY, request.insertBetween)) {
            switchToMacroView();
        }
    });
    graphComponent_.setNodeSwapHandler([this](const std::string& first, const std::string& second) {
        if (app_.swapMacroNodes(first, second)) {
            switchToMacroView();
        }
    });
    graphComponent_.setNodeInsertHandler([this](const std::string& insertNode, const std::pair<std::string, std::string>& connection) {
        if (app_.insertNodeIntoConnection(insertNode, connection)) {
            switchToMacroView();
        }
    });
    graphComponent_.setFixedEndpoints(std::nullopt, std::nullopt);
    nodeLibrary_.setTheme(app_.nodeGraphView().theme());
    updateBreadcrumbs();
    selectedNode_.reset();
    graphComponent_.grabKeyboardFocus();
    refreshIoConfigPanel();
}

void MainComponent::updateBreadcrumbs() {
    refreshBreadcrumbBar();
}

void MainComponent::navigateBack() {
    if (breadcrumbStack_.empty()) {
        switchToMacroView();
        return;
    }

    const int targetIndex = static_cast<int>(breadcrumbStack_.size()) - 1;
    navigateToBreadcrumbIndex(targetIndex);
}

void MainComponent::navigateToBreadcrumbIndex(int index) {
    if (index <= 0) {
        switchToMacroView();
        return;
    }

    const auto clampedIndex = std::min(index, static_cast<int>(breadcrumbStack_.size()));

    switchToMacroView();

    std::vector<std::pair<std::string, std::string>> path;
    path.reserve(static_cast<std::size_t>(clampedIndex));

    for (int i = 0; i < clampedIndex; ++i) {
        const auto& entry = breadcrumbStack_[static_cast<std::size_t>(i)];
        auto descriptor = app_.microViewDescriptor(entry.first);
        switchToMicroView(entry.first, entry.second, descriptor);
    }
}

void MainComponent::refreshBreadcrumbBar() {
    breadcrumbs_.setVisible(false);
    backButton_.setVisible(false);

    for (auto* button : breadcrumbButtons_) {
        breadcrumbBar_.removeChildComponent(button);
    }
    breadcrumbButtons_.clear();

    std::vector<std::pair<std::string, std::string>> path;
    path.insert(path.end(), breadcrumbStack_.begin(), breadcrumbStack_.end());
    path.insert(path.begin(), std::make_pair(std::string(), "Home"));

    auto bounds = breadcrumbBar_.getLocalBounds();
    const int height = bounds.getHeight();
    const int spacing = 8;
    int x = 0;

    const auto& theme = app_.nodeGraphView().theme();
    auto pillColour = toColour(theme.accent).withAlpha(0.2F);
    auto pillColourPressed = toColour(theme.accent).withAlpha(0.4F);
    auto textColour = toColour(theme.textPrimary);

    for (int i = 0; i < static_cast<int>(path.size()); ++i) {
        auto* button = breadcrumbButtons_.add(new juce::TextButton(path[static_cast<std::size_t>(i)].second));
        button->setClickingTogglesState(false);
        button->setColour(juce::TextButton::buttonColourId, pillColour);
        button->setColour(juce::TextButton::buttonOnColourId, pillColourPressed);
        button->setColour(juce::TextButton::textColourOffId, textColour);
        button->setColour(juce::TextButton::textColourOnId, textColour);
        button->onClick = [this, index = i]() { navigateToBreadcrumbIndex(index); };
        const auto font = button->getLookAndFeel().getTextButtonFont(*button, height);
        juce::GlyphArrangement glyph;
        glyph.addLineOfText(font, button->getButtonText(), 0.0F, 0.0F);
        const int width = std::max(70, static_cast<int>(glyph.getBoundingBox(0, -1, true).getWidth() + 24.0F));
        button->setBounds(x, 0, width, height);
        button->setConnectedEdges(juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);
        breadcrumbBar_.addAndMakeVisible(button);
        x += width + spacing;
    }
}

void MainComponent::refreshIoConfigPanel() {
    suppressIoEvents_ = true;

    const auto hidePanel = [&]() {
        ioGroup_.setVisible(false);
        inputLabel_.setVisible(false);
        inputFormatBox_.setVisible(false);
        outputLabel_.setVisible(false);
        outputFormatBox_.setVisible(false);
        suppressIoEvents_ = false;
        resized();
    };

    if (!selectedNode_) {
        hidePanel();
        return;
    }

    const auto nodeTypeOpt = app_.nodeTypeForId(*selectedNode_);
    if (!nodeTypeOpt) {
        hidePanel();
        return;
    }

    const auto nodeInfo = app_.nodeForId(*selectedNode_);
    if (!nodeInfo) {
        hidePanel();
        return;
    }

    const auto nodeType = *nodeTypeOpt;
    const bool isChannel = nodeType == audio::GraphNodeType::Channel;
    const bool isOutput = nodeType == audio::GraphNodeType::Output;
    if (!isChannel && !isOutput) {
        hidePanel();
        return;
    }

    ioGroup_.setVisible(true);
    outputLabel_.setVisible(true);
    outputFormatBox_.setVisible(true);

    const auto configureCombo = [&](juce::ComboBox& box, std::uint32_t channels) {
        box.clear(juce::dontSendNotification);
        box.addItem("Mono (1 channel)", 1);
        box.addItem("Stereo (2 channels)", 2);
        const int selectedId = (channels >= 2) ? 2 : 1;
        box.setSelectedId(selectedId, juce::dontSendNotification);
    };

    if (isChannel) {
        inputLabel_.setVisible(true);
        inputFormatBox_.setVisible(true);
        configureCombo(inputFormatBox_, std::max<std::uint32_t>(1U, nodeInfo->inputChannelCount()));
        configureCombo(outputFormatBox_, std::max<std::uint32_t>(1U, nodeInfo->outputChannelCount()));
    } else {
        inputLabel_.setVisible(false);
        inputFormatBox_.setVisible(false);
        configureCombo(outputFormatBox_, std::max<std::uint32_t>(1U, nodeInfo->inputChannelCount()));
    }

    suppressIoEvents_ = false;
    resized();
}

ui::NodeGraphView::PositionOverrideMap MainComponent::buildOverrides(const std::unordered_map<std::string, persistence::LayoutPosition>& layout) const {
    ui::NodeGraphView::PositionOverrideMap overrides;
    overrides.reserve(layout.size());
    for (const auto& [id, position] : layout) {
        overrides.emplace(id, ui::NodeGraphView::PositionOverride { position.normX, position.normY });
    }
    return overrides;
}

std::string MainComponent::labelForNode(const std::string& nodeId) const {
    if (const auto nodeOpt = app_.nodeForId(nodeId)) {
        if (!nodeOpt->label().empty()) {
            return nodeOpt->label();
        }
    }

    const auto findLabel = [&](const auto& nodes) -> std::optional<std::string> {
        if (nodes.empty()) {
            return std::nullopt;
        }

        const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
            return node.id == nodeId && !node.label.empty();
        });
        if (it != nodes.end()) {
            return it->label;
        }
        return std::nullopt;
    };

    if (const auto macroLabel = findLabel(app_.nodeGraphView().nodes())) {
        return *macroLabel;
    }

    if (currentMicro_ && currentMicro_->view) {
        if (const auto microLabel = findLabel(currentMicro_->view->nodes())) {
            return *microLabel;
        }
    }

    return nodeId;
}

std::optional<core::Application::NodeTemplate> MainComponent::templateForLibraryId(const std::string& id) const {
    const auto normalised = juce::String(id).toLowerCase();

    if (normalised == "channel") {
        return core::Application::NodeTemplate::Channel;
    }
    if (normalised == "output") {
        return core::Application::NodeTemplate::Output;
    }
    if (normalised == "group") {
        return core::Application::NodeTemplate::Group;
    }
    if (normalised == "effect") {
        return core::Application::NodeTemplate::Effect;
    }
    if (normalised == "signal_generator") {
        return core::Application::NodeTemplate::SignalGenerator;
    }

    return std::nullopt;
}

bool MainComponent::isChannelNode(const std::string& nodeId) const {
    if (const auto topology = app_.graphTopology()) {
        if (const auto node = topology->findNode(nodeId)) {
            return node->type() == audio::GraphNodeType::Channel;
        }
    }
    return false;
}

void MainComponent::comboBoxChanged(juce::ComboBox* comboBox) {
    if (suppressIoEvents_) {
        return;
    }

    if (!comboBox || !selectedNode_) {
        return;
    }

    const auto nodeTypeOpt = app_.nodeTypeForId(*selectedNode_);
    const auto nodeInfo = app_.nodeForId(*selectedNode_);
    if (!nodeTypeOpt || !nodeInfo) {
        return;
    }

    const auto nodeType = *nodeTypeOpt;
    if (nodeType != audio::GraphNodeType::Channel && nodeType != audio::GraphNodeType::Output) {
        return;
    }

    std::uint32_t desiredInputChannels = nodeInfo->inputChannelCount();
    std::uint32_t desiredOutputChannels = nodeInfo->outputChannelCount();

    if (comboBox == &inputFormatBox_) {
        const auto selectedId = inputFormatBox_.getSelectedId();
        desiredInputChannels = selectedId == 2 ? 2U : 1U;
    }

    if (comboBox == &outputFormatBox_) {
        const auto selectedId = outputFormatBox_.getSelectedId();
        const auto channels = selectedId == 2 ? 2U : 1U;
        if (nodeType == audio::GraphNodeType::Output) {
            desiredInputChannels = channels;
        } else {
            desiredOutputChannels = channels;
        }
    }

    bool updated = false;
    if (nodeType == audio::GraphNodeType::Channel) {
        updated = app_.configureNodeChannels(*selectedNode_, desiredInputChannels, desiredOutputChannels);
    } else if (nodeType == audio::GraphNodeType::Output) {
        updated = app_.configureNodeChannels(*selectedNode_, desiredInputChannels, 0);
    }

    if (updated) {
        refreshIoConfigPanel();
    }
}

} // namespace broadcastmix::app
