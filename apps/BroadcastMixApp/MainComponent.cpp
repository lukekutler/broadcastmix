#include "MainComponent.h"

#include <core/Application.h>
#include <persistence/ProjectSerializer.h>

#include <audio/GraphNode.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <sstream>
#include <iostream>

namespace broadcastmix::app {

namespace {
constexpr auto kHeadlineText = "BroadcastMix v3";
constexpr auto kSubText = "Drag nodes from the library to build your mix.";

juce::Image loadImageFromPath(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    juce::File file { juce::String(path) };
    if (!file.existsAsFile()) {
        return {};
    }
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) {
        return {};
    }
    return juce::ImageFileFormat::loadFrom(stream);
}

std::string initialsFromName(const std::string& name) {
    std::string initials;
    std::istringstream stream(name);
    std::string word;
    while (stream >> word && initials.size() < 2) {
        for (char ch : word) {
            if (std::isalpha(static_cast<unsigned char>(ch))) {
                initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                break;
            }
        }
    }
    if (initials.empty()) {
        for (char ch : name) {
            if (std::isalpha(static_cast<unsigned char>(ch))) {
                initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                break;
            }
        }
    }
    if (initials.size() == 1 && name.size() > 1) {
        for (auto it = name.rbegin(); it != name.rend(); ++it) {
            if (std::isalpha(static_cast<unsigned char>(*it))) {
                initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*it))));
                break;
            }
        }
    }
    if (initials.size() > 2) {
        initials.resize(2);
    }
    return initials;
}
} // namespace

void MainComponent::AvatarComponent::setTheme(juce::Colour fill, juce::Colour outline, juce::Colour text) {
    fillColour_ = fill;
    outlineColour_ = outline;
    textColour_ = text;
    repaint();
}

void MainComponent::AvatarComponent::setImage(const juce::Image& image) {
    image_ = image;
    repaint();
}

void MainComponent::AvatarComponent::clearImage() {
    image_ = {};
    repaint();
}

void MainComponent::AvatarComponent::setInitials(juce::String initials) {
    initials_ = initials.trim();
    repaint();
}

void MainComponent::AvatarComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::transparentBlack);
    if (getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    auto bounds = getLocalBounds().toFloat().reduced(1.0F);
    const float diameter = std::min(bounds.getWidth(), bounds.getHeight());
    auto circle = juce::Rectangle<float>(0.0F, 0.0F, diameter, diameter).withCentre(bounds.getCentre());

    g.setColour(fillColour_);
    g.fillEllipse(circle);

    if (image_.isValid()) {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addEllipse(circle);
        g.reduceClipRegion(clip);
        g.drawImageWithin(image_,
                          static_cast<int>(std::floor(circle.getX())),
                          static_cast<int>(std::floor(circle.getY())),
                          static_cast<int>(std::round(circle.getWidth())),
                          static_cast<int>(std::round(circle.getHeight())),
                          juce::RectanglePlacement::fillDestination);
    } else if (initials_.isNotEmpty()) {
        g.setColour(textColour_);
        const float fontHeight = diameter * 0.45F;
        g.setFont(juce::Font(juce::FontOptions { fontHeight, juce::Font::bold }));
        g.drawFittedText(initials_,
                         circle.toNearestInt(),
                         juce::Justification::centred,
                         1);
    }

    g.setColour(outlineColour_);
    g.drawEllipse(circle, 1.8F);
}

MainComponent::MainComponent(core::Application& app)
    : app_(app)
    , graphComponent_(&app.nodeGraphView()) {
    setOpaque(true);

    graphComponent_.setNodeDoubleClickHandler([this](const std::string& nodeId) {
        handleNodeDoubleClick(nodeId);
    });
    graphComponent_.setSelectionChangedHandler([this](const std::optional<std::string>& nodeId) {
        selectedNode_ = nodeId;
        refreshSetupPanel();
    });
    graphComponent_.setNodeRenameHandler([this](const std::string& nodeId, const std::string& newLabel) {
        if (app_.renameNode(nodeId, newLabel)) {
            handleRenameSuccess(nodeId);
        }
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
    addAndMakeVisible(setupGroup_);
    addAndMakeVisible(inputLabel_);
    addAndMakeVisible(inputFormatBox_);
    addAndMakeVisible(outputLabel_);
    addAndMakeVisible(outputFormatBox_);
    addAndMakeVisible(personLabel_);
    addAndMakeVisible(personEditor_);
    addAndMakeVisible(roleLabel_);
    addAndMakeVisible(roleEditor_);
    addAndMakeVisible(presetLabel_);
    addAndMakeVisible(presetBox_);
    addAndMakeVisible(savePresetButton_);
    addAndMakeVisible(profileLabel_);
    addAndMakeVisible(avatarPreview_);
    addAndMakeVisible(chooseImageButton_);
    addAndMakeVisible(clearImageButton_);
    addAndMakeVisible(breadcrumbBar_);

    setupGroup_.setVisible(false);
    inputLabel_.setVisible(false);
    inputFormatBox_.setVisible(false);
    outputLabel_.setVisible(false);
    outputFormatBox_.setVisible(false);
    personLabel_.setVisible(false);
    personEditor_.setVisible(false);
    roleLabel_.setVisible(false);
    roleEditor_.setVisible(false);
    presetLabel_.setVisible(false);
    presetBox_.setVisible(false);
    savePresetButton_.setVisible(false);
    profileLabel_.setVisible(false);
    avatarPreview_.setVisible(false);
    chooseImageButton_.setVisible(false);
    clearImageButton_.setVisible(false);

    inputLabel_.setJustificationType(juce::Justification::centredLeft);
    outputLabel_.setJustificationType(juce::Justification::centredLeft);
    personLabel_.setJustificationType(juce::Justification::centredLeft);
    roleLabel_.setJustificationType(juce::Justification::centredLeft);
    presetLabel_.setJustificationType(juce::Justification::centredLeft);
    profileLabel_.setJustificationType(juce::Justification::centredLeft);

    inputFormatBox_.addListener(this);
    outputFormatBox_.addListener(this);
    presetBox_.addListener(this);
    inputFormatBox_.setJustificationType(juce::Justification::centredLeft);
    outputFormatBox_.setJustificationType(juce::Justification::centredLeft);
    presetBox_.setJustificationType(juce::Justification::centredLeft);
    presetBox_.setTextWhenNothingSelected("Custom");

    personEditor_.addListener(this);
    roleEditor_.addListener(this);
    personEditor_.setSelectAllWhenFocused(true);
    roleEditor_.setSelectAllWhenFocused(true);
    personEditor_.setFont(juce::Font(juce::FontOptions { 15.0F }));
    roleEditor_.setFont(juce::Font(juce::FontOptions { 14.0F }));

    chooseImageButton_.onClick = [this]() { chooseProfileImage(); };
    clearImageButton_.onClick = [this]() { clearProfileImage(); };
    clearImageButton_.setEnabled(false);
    savePresetButton_.onClick = [this]() { saveCurrentPositionPreset(); };

    const auto& theme = app_.nodeGraphView().theme();
    const auto fill = toColour(theme.accent).withAlpha(0.25F);
    const auto outline = toColour(theme.accent);
    const auto textColour = toColour(theme.textPrimary);
    avatarPreview_.setTheme(fill, outline, textColour);
    const auto placeholderColour = textColour.withAlpha(0.4F);
    personEditor_.setTextToShowWhenEmpty("Add a name", placeholderColour);
    roleEditor_.setTextToShowWhenEmpty("Add a role", placeholderColour);

    nodeLibrary_.setTheme(app_.nodeGraphView().theme());

    switchToMacroView();
    refreshSetupPanel();
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
    const bool showPositionControls = personEditor_.isVisible();
    const bool showAudioControls = inputFormatBox_.isVisible() || outputFormatBox_.isVisible();
    int setupHeight = 0;
    if (setupGroup_.isVisible()) {
        if (showPositionControls) {
            setupHeight = 260;
        } else if (showAudioControls) {
            setupHeight = 140;
        }
    }
    juce::Rectangle<int> configArea;
    if (setupHeight > 0) {
        configArea = libraryArea.removeFromBottom(setupHeight);
        setupGroup_.setBounds(configArea);
        auto content = configArea.reduced(12);

        if (showPositionControls) {
            inputLabel_.setBounds({});
            inputFormatBox_.setBounds({});
            outputLabel_.setBounds({});
            outputFormatBox_.setBounds({});
            auto profileHeader = content.removeFromTop(18);
            profileLabel_.setBounds(profileHeader.removeFromLeft(120));
            content.removeFromTop(6);

            auto avatarRow = content.removeFromTop(88);
            const int avatarSize = 88;
            avatarPreview_.setBounds(avatarRow.removeFromLeft(avatarSize));
            avatarRow.removeFromLeft(12);
            chooseImageButton_.setBounds(avatarRow.removeFromTop(32));
            avatarRow.removeFromTop(6);
            clearImageButton_.setBounds(avatarRow.removeFromTop(24));
            content.removeFromTop(12);

            const int labelWidth = 90;
            auto personRow = content.removeFromTop(28);
            personLabel_.setBounds(personRow.removeFromLeft(labelWidth));
            personRow.removeFromLeft(6);
            personEditor_.setBounds(personRow);
            content.removeFromTop(8);

            auto roleRow = content.removeFromTop(28);
            roleLabel_.setBounds(roleRow.removeFromLeft(labelWidth));
            roleRow.removeFromLeft(6);
            roleEditor_.setBounds(roleRow);
            content.removeFromTop(8);

            auto presetRow = content.removeFromTop(28);
            presetLabel_.setBounds(presetRow.removeFromLeft(labelWidth));
            presetRow.removeFromLeft(6);
            const int spacing = 8;
            const int available = presetRow.getWidth();
            int comboWidth = std::max(120, available - (spacing + 110));
            comboWidth = std::min(comboWidth, available);
            presetBox_.setBounds(presetRow.removeFromLeft(comboWidth));
            if (presetRow.getWidth() > spacing) {
                presetRow.removeFromLeft(spacing);
            }
            savePresetButton_.setBounds(presetRow);
        } else if (showAudioControls) {
            auto contentAudio = content;
            if (inputFormatBox_.isVisible()) {
                auto inputRow = contentAudio.removeFromTop(28);
                inputLabel_.setBounds(inputRow.removeFromLeft(70));
                inputRow.removeFromLeft(6);
                inputFormatBox_.setBounds(inputRow);
                contentAudio.removeFromTop(8);
            } else {
                inputLabel_.setBounds({});
                inputFormatBox_.setBounds({});
            }

            if (outputFormatBox_.isVisible()) {
                auto outputRow = contentAudio.removeFromTop(28);
                outputLabel_.setBounds(outputRow.removeFromLeft(70));
                outputRow.removeFromLeft(6);
                outputFormatBox_.setBounds(outputRow);
            } else {
                outputLabel_.setBounds({});
                outputFormatBox_.setBounds({});
            }

            profileLabel_.setBounds({});
            avatarPreview_.setBounds({});
            chooseImageButton_.setBounds({});
            clearImageButton_.setBounds({});
            personLabel_.setBounds({});
            personEditor_.setBounds({});
            roleLabel_.setBounds({});
            roleEditor_.setBounds({});
            presetLabel_.setBounds({});
            presetBox_.setBounds({});
            savePresetButton_.setBounds({});
        }
    } else {
        setupGroup_.setBounds({});
        inputLabel_.setBounds({});
        inputFormatBox_.setBounds({});
        outputLabel_.setBounds({});
        outputFormatBox_.setBounds({});
        profileLabel_.setBounds({});
        avatarPreview_.setBounds({});
        chooseImageButton_.setBounds({});
        clearImageButton_.setBounds({});
        personLabel_.setBounds({});
        personEditor_.setBounds({});
        roleLabel_.setBounds({});
        roleEditor_.setBounds({});
        presetLabel_.setBounds({});
        presetBox_.setBounds({});
        savePresetButton_.setBounds({});
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

    const auto character = key.getTextCharacter();
    if ((character == 'r' || character == 'R') && selectedNode_) {
        graphComponent_.beginNodeRename(*selectedNode_);
        return true;
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
    if ((macroType == audio::GraphNodeType::Channel || macroType == audio::GraphNodeType::GroupBus || macroType == audio::GraphNodeType::Position || macroType == audio::GraphNodeType::Output) && outputExists) {
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
    refreshSetupPanel();
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
    refreshSetupPanel();
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

    std::vector<std::pair<std::string, std::string>> path;
    path.reserve(static_cast<std::size_t>(clampedIndex));
    for (int i = 0; i < clampedIndex; ++i) {
        path.emplace_back(breadcrumbStack_[static_cast<std::size_t>(i)]);
    }

    switchToMacroView();

    for (const auto& entry : path) {
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
        const juce::String buttonText(path[static_cast<std::size_t>(i)].second);
        auto* button = breadcrumbButtons_.add(new juce::TextButton(buttonText));
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

void MainComponent::refreshSetupPanel() {
    suppressSetupEvents_ = true;

    const auto hidePanel = [&]() {
        setupGroup_.setVisible(false);
        inputLabel_.setVisible(false);
        inputFormatBox_.setVisible(false);
        outputLabel_.setVisible(false);
        outputFormatBox_.setVisible(false);
        personLabel_.setVisible(false);
        personEditor_.setVisible(false);
        roleLabel_.setVisible(false);
        roleEditor_.setVisible(false);
        presetLabel_.setVisible(false);
        presetBox_.setVisible(false);
        savePresetButton_.setVisible(false);
        profileLabel_.setVisible(false);
        avatarPreview_.setVisible(false);
        chooseImageButton_.setVisible(false);
        clearImageButton_.setVisible(false);
        clearImageButton_.setEnabled(false);
        currentProfileImagePath_.clear();
        suppressSetupEvents_ = false;
        resized();
    };

    if (!selectedNode_) {
        hidePanel();
        return;
    }

    const auto nodeTypeOpt = app_.nodeTypeForId(*selectedNode_);
    const auto nodeInfo = app_.nodeForId(*selectedNode_);
    if (!nodeTypeOpt || !nodeInfo) {
        hidePanel();
        return;
    }

    const auto nodeType = *nodeTypeOpt;

    if (nodeType == audio::GraphNodeType::Channel || nodeType == audio::GraphNodeType::Output) {
        setupGroup_.setVisible(true);

        const auto configureCombo = [&](juce::ComboBox& box, std::uint32_t channels) {
            box.clear(juce::dontSendNotification);
            box.addItem("Mono (1 channel)", 1);
            box.addItem("Stereo (2 channels)", 2);
            const int selectedId = (channels >= 2) ? 2 : 1;
            box.setSelectedId(selectedId, juce::dontSendNotification);
        };

        const bool showInputCombo = nodeType == audio::GraphNodeType::Channel;
        inputLabel_.setVisible(showInputCombo);
        inputFormatBox_.setVisible(showInputCombo);
        if (showInputCombo) {
            configureCombo(inputFormatBox_, std::max<std::uint32_t>(1U, nodeInfo->inputChannelCount()));
        }

        outputLabel_.setVisible(true);
        outputFormatBox_.setVisible(true);
        const auto outputChannels = nodeType == audio::GraphNodeType::Output
            ? std::max<std::uint32_t>(1U, nodeInfo->inputChannelCount())
            : std::max<std::uint32_t>(1U, nodeInfo->outputChannelCount());
        configureCombo(outputFormatBox_, outputChannels);

        personLabel_.setVisible(false);
        personEditor_.setVisible(false);
        roleLabel_.setVisible(false);
        roleEditor_.setVisible(false);
        presetLabel_.setVisible(false);
        presetBox_.setVisible(false);
        savePresetButton_.setVisible(false);
        profileLabel_.setVisible(false);
        avatarPreview_.setVisible(false);
        chooseImageButton_.setVisible(false);
        clearImageButton_.setVisible(false);
        clearImageButton_.setEnabled(false);
        currentProfileImagePath_.clear();
    } else if (nodeType == audio::GraphNodeType::Position) {
        setupGroup_.setVisible(true);

        inputLabel_.setVisible(false);
        inputFormatBox_.setVisible(false);
        outputLabel_.setVisible(false);
        outputFormatBox_.setVisible(false);

        personLabel_.setVisible(true);
        personEditor_.setVisible(true);
        roleLabel_.setVisible(true);
        roleEditor_.setVisible(true);
        presetLabel_.setVisible(true);
        presetBox_.setVisible(true);
        savePresetButton_.setVisible(true);
        profileLabel_.setVisible(true);
        avatarPreview_.setVisible(true);
        chooseImageButton_.setVisible(true);
        clearImageButton_.setVisible(true);

        personEditor_.setText(juce::String(nodeInfo->person()), juce::dontSendNotification);
        roleEditor_.setText(juce::String(nodeInfo->role()), juce::dontSendNotification);

        presetBox_.clear(juce::dontSendNotification);
        presetBox_.addItem("Custom", 1);
        const auto presets = app_.positionPresetNames();
        int itemId = 2;
        int selectedId = 1;
        for (const auto& presetName : presets) {
            presetBox_.addItem(presetName, itemId);
            if (presetName == nodeInfo->presetName()) {
                selectedId = itemId;
            }
            ++itemId;
        }
        if (selectedId == 1 && !nodeInfo->presetName().empty()) {
            presetBox_.addItem(nodeInfo->presetName(), itemId);
            selectedId = itemId;
        }
        presetBox_.setSelectedId(selectedId, juce::dontSendNotification);

        savePresetButton_.setEnabled(true);

        updateAvatarDisplay(*nodeInfo);
    } else {
        hidePanel();
        return;
    }

    suppressSetupEvents_ = false;
    resized();
}

void MainComponent::updateAvatarDisplay(const audio::GraphNode& node) {
    const auto initials = initialsFromName(node.person());
    avatarPreview_.setInitials(juce::String(initials));

    const auto imagePath = node.profileImagePath();
    currentProfileImagePath_ = imagePath;
    const auto image = loadImageFromPath(imagePath);
    if (image.isValid()) {
        avatarPreview_.setImage(image);
    } else {
        avatarPreview_.clearImage();
    }
    clearImageButton_.setEnabled(!imagePath.empty());
}

void MainComponent::handleRenameSuccess(const std::string& nodeId) {
    if (currentMicro_) {
        auto descriptor = app_.microViewDescriptor(currentMicro_->id);
        switchToMicroView(currentMicro_->id,
                          labelForNode(currentMicro_->id),
                          descriptor);
    } else {
        graphComponent_.setGraphView(&app_.nodeGraphView());
        updateBreadcrumbs();
    }
    refreshSetupPanel();
}

void MainComponent::applyPersonUpdate() {
    if (suppressSetupEvents_ || !selectedNode_) {
        return;
    }
    const auto nodeType = app_.nodeTypeForId(*selectedNode_);
    if (!nodeType || *nodeType != audio::GraphNodeType::Position) {
        return;
    }
    const auto text = personEditor_.getText();
    if (app_.updatePositionPerson(*selectedNode_, text.toStdString())) {
        presetBox_.setSelectedId(1, juce::dontSendNotification);
        updateBreadcrumbs();
        if (currentMicro_ && currentMicro_->id == *selectedNode_) {
            currentMicro_->label = text.toStdString();
        }
    }
}

void MainComponent::applyRoleUpdate() {
    if (suppressSetupEvents_ || !selectedNode_) {
        return;
    }
    const auto nodeType = app_.nodeTypeForId(*selectedNode_);
    if (!nodeType || *nodeType != audio::GraphNodeType::Position) {
        return;
    }
    const auto text = roleEditor_.getText();
    if (app_.updatePositionRole(*selectedNode_, text.toStdString(), false)) {
        presetBox_.setSelectedId(1, juce::dontSendNotification);
    }
}

void MainComponent::chooseProfileImage() {
    if (suppressSetupEvents_ || !selectedNode_) {
        return;
    }
    const auto nodeType = app_.nodeTypeForId(*selectedNode_);
    if (!nodeType || *nodeType != audio::GraphNodeType::Position) {
        return;
    }

    if (activeFileChooser_) {
        return;
    }

    const auto nodeId = *selectedNode_;
    auto chooser = std::make_unique<juce::FileChooser>("Select Profile Image",
                                                       juce::File(),
                                                       "*.png;*.jpg;*.jpeg;*.bmp;*.gif");
    auto* chooserPtr = chooser.get();
    activeFileChooser_ = std::move(chooser);
    chooserPtr->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, nodeId](const juce::FileChooser& fc) {
                                 auto file = fc.getResult();
                                 activeFileChooser_.reset();
                                 if (!file.existsAsFile()) {
                                     return;
                                 }
                                 if (!selectedNode_ || *selectedNode_ != nodeId) {
                                     return;
                                 }
                                 if (app_.updatePositionProfileImage(nodeId, file.getFullPathName().toStdString(), false)) {
                                     presetBox_.setSelectedId(1, juce::dontSendNotification);
                                     refreshSetupPanel();
                                 }
                             });
}

void MainComponent::clearProfileImage() {
    if (suppressSetupEvents_ || !selectedNode_) {
        return;
    }
    const auto nodeType = app_.nodeTypeForId(*selectedNode_);
    if (!nodeType || *nodeType != audio::GraphNodeType::Position) {
        return;
    }

    if (app_.updatePositionProfileImage(*selectedNode_, std::string {}, false)) {
        presetBox_.setSelectedId(1, juce::dontSendNotification);
        refreshSetupPanel();
    } else if (!currentProfileImagePath_.empty()) {
        refreshSetupPanel();
    }
}

void MainComponent::saveCurrentPositionPreset() {
    if (!selectedNode_) {
        return;
    }
    const auto nodeType = app_.nodeTypeForId(*selectedNode_);
    if (!nodeType || *nodeType != audio::GraphNodeType::Position) {
        return;
    }

    auto* alert = new juce::AlertWindow("Save Preset",
                                        "Enter preset name:",
                                        juce::AlertWindow::NoIcon);
    alert->addTextEditor("name", personEditor_.getText(), "Preset Name:");
    alert->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    alert->centreAroundComponent(this, 360, 180);

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Component::SafePointer<juce::AlertWindow> safeAlert(alert);

    alert->enterModalState(true,
        juce::ModalCallbackFunction::create([safeThis, safeAlert](int result) {
            if (safeThis == nullptr || safeAlert == nullptr || result != 1) {
                return;
            }
            if (!safeThis->selectedNode_) {
                return;
            }
            const auto presetName = safeAlert->getTextEditorContents("name").trim();
            if (presetName.isEmpty()) {
                return;
            }
            if (safeThis->app_.savePositionPreset(*safeThis->selectedNode_, presetName.toStdString())) {
                safeThis->refreshSetupPanel();
            }
        }),
        true);
}

void MainComponent::textEditorTextChanged(juce::TextEditor& editor) {
    if (suppressSetupEvents_) {
        return;
    }
    if (&editor == &personEditor_) {
        avatarPreview_.setInitials(juce::String(initialsFromName(personEditor_.getText().toStdString())));
        applyPersonUpdate();
    } else if (&editor == &roleEditor_) {
        applyRoleUpdate();
    }
}

void MainComponent::textEditorReturnKeyPressed(juce::TextEditor& editor) {
    if (suppressSetupEvents_) {
        return;
    }
    if (&editor == &personEditor_) {
        applyPersonUpdate();
    } else if (&editor == &roleEditor_) {
        applyRoleUpdate();
    }
}

void MainComponent::textEditorFocusLost(juce::TextEditor& editor) {
    if (suppressSetupEvents_) {
        return;
    }
    if (&editor == &personEditor_) {
        applyPersonUpdate();
    } else if (&editor == &roleEditor_) {
        applyRoleUpdate();
    }
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
    if (normalised == "position") {
        return core::Application::NodeTemplate::Position;
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
    if (suppressSetupEvents_) {
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

    if (comboBox == &presetBox_) {
        if (*nodeTypeOpt != audio::GraphNodeType::Position) {
            return;
        }
        const auto selectedId = presetBox_.getSelectedId();
        if (selectedId <= 1) {
            presetBox_.setSelectedId(1, juce::dontSendNotification);
            if (app_.clearPositionPreset(*selectedNode_)) {
                refreshSetupPanel();
            }
            return;
        }
        const auto presetName = presetBox_.getText();
        if (presetName.isNotEmpty() && app_.applyPositionPreset(*selectedNode_, presetName.toStdString())) {
            refreshSetupPanel();
            if (currentMicro_ && currentMicro_->id == *selectedNode_) {
                auto descriptor = app_.microViewDescriptor(currentMicro_->id);
                switchToMicroView(currentMicro_->id,
                                  labelForNode(currentMicro_->id),
                                  descriptor);
            }
        }
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
        refreshSetupPanel();
    }
}

} // namespace broadcastmix::app
