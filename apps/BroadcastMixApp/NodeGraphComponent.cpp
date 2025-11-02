#include "NodeGraphComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

namespace broadcastmix::app {

namespace {
constexpr float kNodeWidth = 140.0F;
constexpr float kNodeHeight = 70.0F;
constexpr float kCornerRadius = 16.0F;
constexpr float kHorizontalPadding = 48.0F;
constexpr float kVerticalPadding = 36.0F;
constexpr float kPortRadius = 5.0F;
constexpr float kPortHitRadius = 9.0F;
constexpr float kConnectionDropTolerance = 12.0F;
constexpr float kNormPadding = 0.1F;
constexpr float kNormMin = -0.25F;
constexpr float kNormMax = 2.0F;
constexpr int kMinMacroCanvasWidth = 2400;
constexpr int kMinMacroCanvasHeight = 1600;
constexpr int kMinMicroCanvasWidth = 1200;
constexpr int kMinMicroCanvasHeight = 900;
constexpr float kMicroNormMin = -0.05F;
constexpr float kMicroNormMax = 1.05F;
constexpr float kMicroNormMinY = 0.0F;
constexpr float kMicroNormMaxY = 1.0F;

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
    if (initials.size() == 1) {
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

juce::Image loadAvatarImage(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    juce::File file { juce::String(path) };
    if (!file.existsAsFile()) {
        return {};
    }
    return juce::ImageCache::getFromFile(file);
}
} // namespace

NodeGraphComponent::NodeGraphComponent(ui::NodeGraphView* view)
    : view_(view) {
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);
    setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    setMouseClickGrabsKeyboardFocus(true);
    startTimerHz(15);
}

void NodeGraphComponent::paint(juce::Graphics& g) {
    if (view_ == nullptr) {
        g.fillAll(juce::Colour::fromFloatRGBA(0.07F, 0.07F, 0.09F, 1.0F));
        return;
    }

    refreshCachedPositions();
    const auto area = layoutArea_;

    const auto& theme = view_->theme();
    const auto backgroundColour = toColour(theme.background);
    const auto accentColour = toColour(theme.accent);
    const auto textPrimaryColour = toColour(theme.textPrimary);
    const auto meterPeakColour = toColour(theme.meterPeak);

    g.fillAll(backgroundColour);

    // Draw grid backdrop.
    g.setColour(backgroundColour.brighter(0.08F));
    const auto gridSpacing = 32.0F;
    for (float x = area.getX(); x <= area.getRight(); x += gridSpacing) {
        g.drawLine(x, area.getY(), x, area.getBottom(), 0.5F);
    }
    for (float y = area.getY(); y <= area.getBottom(); y += gridSpacing) {
        g.drawLine(area.getX(), y, area.getRight(), y, 0.5F);
    }

    inputPortPositions_.clear();
    outputPortPositions_.clear();
    fixedInputAnchor_.reset();
    fixedOutputAnchor_.reset();
    fixedInputNormY_.reset();
    fixedOutputNormY_.reset();

    if (view_->nodes().empty()) {
        g.setColour(textPrimaryColour.withAlpha(0.55F));
        g.setFont(juce::Font(juce::FontOptions { 16.0F }));
        g.drawFittedText("Drag nodes from the library",
                         area.toNearestInt(),
                         juce::Justification::centred,
                         1);
    }

    const auto clipBounds = g.getClipBounds().toFloat().expanded(24.0F);
    juce::DropShadow nodeShadow(backgroundColour.darker(0.4F), 10, {});

    const auto computePortY = [](const juce::Rectangle<float>& bounds, std::uint32_t count, std::size_t index) {
        if (count == 0) {
            return bounds.getCentreY();
        }
        const auto fraction = (static_cast<float>(index) + 1.0F) / (static_cast<float>(count) + 1.0F);
        return bounds.getY() + fraction * bounds.getHeight();
    };

    for (const auto& nodeVisual : view_->nodes()) {
        const auto posIt = cachedPositions_.find(nodeVisual.id);
        if (posIt == cachedPositions_.end()) {
            continue;
        }

        const auto position = posIt->second;
        const bool isFixedInput = fixedInputEnabled_ &&
            ((fixedInputId_ && nodeVisual.id == *fixedInputId_) || nodeVisual.type == audio::GraphNodeType::Input);
        const bool isFixedOutput = fixedOutputEnabled_ &&
            ((fixedOutputId_ && nodeVisual.id == *fixedOutputId_) || nodeVisual.type == audio::GraphNodeType::Output);

        if (isFixedInput || isFixedOutput) {
            const auto anchorX = isFixedInput ? layoutArea_.getX() + 10.0F : layoutArea_.getRight() - 10.0F;
            const auto anchorY = juce::jlimit(layoutArea_.getY() + 12.0F, layoutArea_.getBottom() - 12.0F, position.y);
            const auto anchor = juce::Point<float>(anchorX, anchorY);
            const float radius = 8.0F;
            juce::Rectangle<float> circleBounds(anchor.x - radius, anchor.y - radius, radius * 2.0F, radius * 2.0F);
            g.setColour(accentColour.withAlpha(0.38F));
            g.fillEllipse(circleBounds);
            g.setColour(accentColour);
            g.drawEllipse(circleBounds, 1.6F);

            g.setColour(textPrimaryColour);
            g.setFont(juce::Font(juce::FontOptions { 11.0F, juce::Font::bold }));
            auto labelBounds = circleBounds.withSizeKeepingCentre(30.0F, 16.0F);
            labelBounds = isFixedInput ? labelBounds.translated(20.0F, -18.0F) : labelBounds.translated(-20.0F, -18.0F);
            g.drawFittedText(isFixedInput ? "IN" : "OUT",
                             labelBounds.toNearestInt(),
                             juce::Justification::centred,
                             1);

            if (isFixedInput) {
                fixedInputId_ = nodeVisual.id;
                fixedInputAnchor_ = anchor;
                inputPortPositions_[nodeVisual.id] = {};
                outputPortPositions_[nodeVisual.id] = { anchor };
            } else {
                fixedOutputId_ = nodeVisual.id;
                fixedOutputAnchor_ = anchor;
                inputPortPositions_[nodeVisual.id] = { anchor };
                outputPortPositions_[nodeVisual.id] = {};
            }
            continue;
        }

        const auto nodeBounds = nodeBoundsForPosition(position);
        if (!clipBounds.intersects(nodeBounds.expanded(12.0F))) {
            continue;
        }

        auto fillColour = nodeFillColour(nodeVisual.type);
        if (!nodeVisual.enabled) {
            fillColour = fillColour.withAlpha(0.35F);
        }

        nodeShadow.drawForRectangle(g, nodeBounds.toNearestInt());

        g.setColour(fillColour);
        g.fillRoundedRectangle(nodeBounds, kCornerRadius);

        g.setColour(textPrimaryColour);
        auto labelBounds = nodeBounds.reduced(12.0F);
        const bool isRenamingNode = renameEditor_ && renamingNodeId_ == nodeVisual.id;
        const bool isPersonNode = nodeVisual.type == audio::GraphNodeType::Person;
        juce::Rectangle<float> renameBounds = labelBounds;
        if (isPersonNode) {
            const juce::String personText(nodeVisual.person.empty() ? nodeVisual.label : nodeVisual.person);
            const juce::String roleText(nodeVisual.role);
            auto textBounds = labelBounds.toNearestInt();

            const float avatarDiameter = 28.0F;
            juce::Rectangle<float> avatarBounds(labelBounds.getX(), labelBounds.getY(), avatarDiameter, avatarDiameter);

            juce::Image avatarImage;
            if (!nodeVisual.profileImagePath.empty()) {
                avatarImage = cachedAvatarForPath(nodeVisual.profileImagePath);
            }

            if (!avatarImage.isValid()) {
                g.setColour(accentColour.withAlpha(0.25F));
                g.fillEllipse(avatarBounds);
                const auto initials = initialsFromName(nodeVisual.person.empty() ? nodeVisual.label : nodeVisual.person);
                if (!initials.empty()) {
                    g.setColour(textPrimaryColour);
                    g.setFont(juce::Font(juce::FontOptions { avatarDiameter * 0.45F, juce::Font::bold }));
                    g.drawFittedText(juce::String(initials),
                                     avatarBounds.toNearestInt(),
                                     juce::Justification::centred,
                                     1);
                }
            } else {
                juce::Graphics::ScopedSaveState stateAvatar(g);
                juce::Path clip;
                clip.addEllipse(avatarBounds);
                g.reduceClipRegion(clip);
                g.drawImageWithin(avatarImage,
                                  static_cast<int>(std::floor(avatarBounds.getX())),
                                  static_cast<int>(std::floor(avatarBounds.getY())),
                                  static_cast<int>(std::round(avatarBounds.getWidth())),
                                  static_cast<int>(std::round(avatarBounds.getHeight())),
                                  juce::RectanglePlacement::fillDestination);
            }
            g.setColour(accentColour);
            g.drawEllipse(avatarBounds, 1.4F);

            textBounds.removeFromLeft(static_cast<int>(avatarDiameter) + 12);
            auto nameBoundsInt = textBounds.removeFromTop(28);
            const auto nameText = personText.isNotEmpty() ? personText : juce::String(nodeVisual.label);
            juce::Font nameFont(juce::FontOptions { 18.0F, juce::Font::bold });
            renameBounds = labelBoundsForText(nodeVisual.id,
                                              nameText.toStdString(),
                                              true,
                                              nameFont,
                                              nameBoundsInt.toFloat(),
                                              juce::Justification::centredLeft);

            if (!isRenamingNode) {
                g.setColour(textPrimaryColour);
                g.setFont(nameFont);
                g.drawFittedText(nameText,
                                 nameBoundsInt,
                                 juce::Justification::centredLeft,
                                 1);
                if (roleText.isNotEmpty()) {
                    textBounds.removeFromTop(4);
                    g.setColour(textPrimaryColour.withAlpha(0.75F));
                    g.setFont(juce::Font(juce::FontOptions { 13.0F, juce::Font::plain }));
                    g.drawFittedText(roleText,
                                     textBounds.removeFromTop(20),
                                     juce::Justification::centredLeft,
                                     1);
                    g.setColour(textPrimaryColour);
                }
            }
        } else {
            juce::Font labelFont(juce::FontOptions { 15.0F, juce::Font::bold });
            renameBounds = labelBoundsForText(nodeVisual.id,
                                              nodeVisual.label,
                                              false,
                                              labelFont,
                                              labelBounds,
                                              juce::Justification::centred);

            if (!isRenamingNode) {
                g.setFont(labelFont);
                g.drawFittedText(nodeVisual.label,
                                 labelBounds.toNearestInt(),
                                 juce::Justification::centred,
                                 1);
            }
        }

        if (isRenamingNode && renameEditor_) {
            renameEditor_->setBounds(renameBounds.toNearestInt());
        }

        if (selectedNodeId_ && *selectedNodeId_ == nodeVisual.id) {
            g.setColour(accentColour);
            g.drawRoundedRectangle(nodeBounds.expanded(4.0F), kCornerRadius + 4.0F, 2.0F);
        }

        if (swapTargetId_ && *swapTargetId_ == nodeVisual.id) {
            g.setColour(accentColour.withAlpha(0.45F));
            g.drawRoundedRectangle(nodeBounds.expanded(6.0F), kCornerRadius + 6.0F, 2.5F);
        }

        if (meterProvider_ && nodeVisual.enabled) {
            const auto levels = meterProvider_(nodeVisual.id);
            const auto level = std::clamp(std::max(levels[0], levels[1]), 0.0F, 1.0F);
            const auto meterWidth = 10.0F;
            const auto meterMargin = 6.0F;
            juce::Rectangle<float> meterBounds {
                nodeBounds.getRight() - meterWidth - meterMargin,
                nodeBounds.getY() + meterMargin,
                meterWidth,
                nodeBounds.getHeight() - (meterMargin * 2.0F)
            };
            auto filledBounds = meterBounds.withTrimmedTop(meterBounds.getHeight() * (1.0F - level));
            g.setColour(meterPeakColour);
            g.fillRect(filledBounds);
            g.setColour(textPrimaryColour.withAlpha(0.3F));
            g.drawRect(meterBounds, 1.0F);
        }

        std::vector<juce::Point<float>> inputPorts;
        std::vector<juce::Point<float>> outputPorts;

        g.setColour(toColour(theme.textPrimary));
        const auto inputPortCount = nodeVisual.inputChannels > 0 ? 1U : 0U;
        for (std::size_t i = 0; i < static_cast<std::size_t>(inputPortCount); ++i) {
            const auto portPoint = juce::Point<float>(
                nodeBounds.getX(),
                computePortY(nodeBounds, inputPortCount, i));
            inputPorts.push_back(portPoint);

            const bool isDragging = draggingPort_ && draggingPort_->nodeId == nodeVisual.id && !draggingPort_->isOutput && draggingPort_->index == i;
            const bool isHover = hoverPort_ && hoverPort_->nodeId == nodeVisual.id && !hoverPort_->isOutput && hoverPort_->index == i;
            auto colour = toColour(theme.textPrimary);
            if (isDragging || isHover) {
                colour = toColour(theme.accent);
            }
            g.setColour(colour);
            g.fillEllipse(portPoint.x - kPortRadius, portPoint.y - kPortRadius,
                          kPortRadius * 2.0F, kPortRadius * 2.0F);
        }
        inputPortPositions_[nodeVisual.id] = std::move(inputPorts);

        g.setColour(toColour(theme.accent));
        const auto outputPortCount = nodeVisual.outputChannels > 0 ? 1U : 0U;
        for (std::size_t i = 0; i < static_cast<std::size_t>(outputPortCount); ++i) {
            const auto portPoint = juce::Point<float>(
                nodeBounds.getRight(),
                computePortY(nodeBounds, outputPortCount, i));
            outputPorts.push_back(portPoint);

            const bool isDragging = draggingPort_ && draggingPort_->nodeId == nodeVisual.id && draggingPort_->isOutput && draggingPort_->index == i;
            const bool isHover = hoverPort_ && hoverPort_->nodeId == nodeVisual.id && hoverPort_->isOutput && hoverPort_->index == i;
            auto colour = toColour(theme.accent);
            if (isDragging || isHover) {
                colour = colour.brighter(0.3F);
            }
            g.setColour(colour);
            g.fillEllipse(portPoint.x - kPortRadius, portPoint.y - kPortRadius,
                          kPortRadius * 2.0F, kPortRadius * 2.0F);
        }
        outputPortPositions_[nodeVisual.id] = std::move(outputPorts);
    }

    connectionSegments_.clear();
    const auto connectionColour = toColour(theme.accent).withAlpha(0.7F);

    std::unordered_set<std::string> drawnConnections;
    drawnConnections.reserve(view_->connections().size());

    for (const auto& connection : view_->connections()) {
        const auto key = connection.fromId + "->" + connection.toId;
        if (!drawnConnections.insert(key).second) {
            continue;
        }

        const auto fromPoint = portPosition(PortSelection { connection.fromId, true, connection.fromPort });
        const auto toPoint = portPosition(PortSelection { connection.toId, false, connection.toPort });

        juce::Line<float> line(fromPoint, toPoint);
        connectionSegments_.push_back(ConnectionSegment { connection.fromId, connection.toId, line });

        const bool isSelected = selectedConnection_ &&
            selectedConnection_->first == connection.fromId &&
            selectedConnection_->second == connection.toId;

        const bool isDropTarget = pendingDropConnection_ &&
            pendingDropConnection_->first == connection.fromId &&
            pendingDropConnection_->second == connection.toId;

        auto colour = connectionColour.withAlpha(0.5F);
        auto thickness = 2.0F;
        if (isSelected) {
            colour = toColour(theme.accent).brighter(0.4F);
            thickness = 3.0F;
        } else if (isDropTarget) {
            colour = toColour(theme.accent).brighter(0.2F);
            thickness = 3.0F;
        }

        g.setColour(colour);
        g.drawLine(line, thickness);
    }

    if (draggingPort_) {
        const auto start = portPosition(*draggingPort_);
        const auto end = hoverPort_ ? portPosition(*hoverPort_) : dragPosition_;
        g.setColour(toColour(theme.accent));
        g.drawLine({ start.x, start.y, end.x, end.y }, 2.0F);
    }

    if (pendingDropPosition_) {
        const auto accent = toColour(theme.accent);
        const auto previewBounds = juce::Rectangle<float>(0.0F, 0.0F, 30.0F, 30.0F).withCentre(*pendingDropPosition_);
        g.setColour(accent.withAlpha(0.2F));
        g.fillEllipse(previewBounds);
        g.setColour(accent.withAlpha(0.85F));
        g.drawEllipse(previewBounds, 2.0F);

        if (pendingDropType_) {
            auto labelBounds = previewBounds.translated(0.0F, -previewBounds.getHeight() - 8.0F).expanded(24.0F, 8.0F);
            g.setColour(toColour(theme.textPrimary));
            g.setFont(juce::Font(juce::FontOptions { 12.0F, juce::Font::bold }));
            juce::String text(*pendingDropType_);
            g.drawFittedText(text.toUpperCase(),
                             labelBounds.toNearestInt(),
                             juce::Justification::centred,
                             1);
        }
    }
}

void NodeGraphComponent::resized() {
    repaint();
}

void NodeGraphComponent::timerCallback() {
    if (view_ == nullptr) {
        return;
    }

    const auto currentVersion = view_->layoutVersion();
    if (currentVersion != lastLayoutVersion_) {
        lastLayoutVersion_ = currentVersion;
        std::cout << "[NodeGraphComponent] layoutVersion changed to " << lastLayoutVersion_ << std::endl;
        repaint();
        return;
    }

    if (meterProvider_) {
        repaint();
    }
}

void NodeGraphComponent::mouseDown(const juce::MouseEvent& event) {
    if (view_ == nullptr) {
        return;
    }

    refreshCachedPositions();

    if (event.mods.isPopupMenu()) {
        if (const auto hitNode = hitTestNode(event.position)) {
            selectedConnection_.reset();
            draggingNodeId_.reset();
            draggingPort_.reset();
            hoverPort_.reset();
            swapTargetId_.reset();
            pendingDropConnection_.reset();
            selectedNodeId_ = *hitNode;
            grabKeyboardFocus();
            if (onSelectionChanged_) {
                onSelectionChanged_(selectedNodeId_);
            }
            repaint();

            juce::PopupMenu menu;
            menu.addItem(1, "Rename...");

            const auto screenPoint = event.getScreenPosition();
            auto options = juce::PopupMenu::Options()
                               .withTargetComponent(this)
                               .withTargetScreenArea({ screenPoint.x, screenPoint.y, 1, 1 });

            juce::Component::SafePointer<NodeGraphComponent> safeThis(this);
            const auto nodeId = *hitNode;
            menu.showMenuAsync(options, [safeThis, nodeId](int result) {
                if (result == 1 && safeThis != nullptr) {
                    safeThis->beginNodeRename(nodeId);
                }
            });
        } else {
            selectedNodeId_.reset();
            selectedConnection_.reset();
            draggingNodeId_.reset();
            draggingPort_.reset();
            hoverPort_.reset();
            swapTargetId_.reset();
            pendingDropConnection_.reset();
            if (onSelectionChanged_) {
                onSelectionChanged_(selectedNodeId_);
            }
            repaint();
        }
        return;
    }

    const float lineHitTolerance = 6.0F;
    for (const auto& segment : connectionSegments_) {
        juce::Point<float> closestPoint;
        const auto distance = segment.line.getDistanceFromPoint(event.position, closestPoint);
        if (distance <= lineHitTolerance) {
            selectedConnection_ = std::make_pair(segment.fromId, segment.toId);
            selectedNodeId_.reset();
            draggingNodeId_.reset();
            draggingPort_.reset();
            hoverPort_.reset();
            if (onSelectionChanged_) {
                onSelectionChanged_(std::nullopt);
            }
            grabKeyboardFocus();
            repaint();
            return;
        }
    }

    const auto portHit = findPortAt(event.position);
    if (portHit) {
        draggingPort_ = portHit;
        dragPosition_ = portPosition(*portHit);
        hoverPort_.reset();
        selectedConnection_.reset();
        const bool isFixedEndpoint = (fixedInputId_ && portHit->nodeId == *fixedInputId_) || (fixedOutputId_ && portHit->nodeId == *fixedOutputId_);
        if (!isFixedEndpoint) {
            selectedNodeId_ = portHit->nodeId;
            juce::Logger::writeToLog("NodeGraphComponent :: selected node -> " + juce::String(*selectedNodeId_));
            grabKeyboardFocus();
            if (onSelectionChanged_) {
                onSelectionChanged_(selectedNodeId_);
            }
        } else {
            selectedNodeId_.reset();
        }
        repaint();
        return;
    }

    const auto hitNode = hitTestNode(event.position);
    if (hitNode) {
        selectedConnection_.reset();
        if (selectedNodeId_ && *selectedNodeId_ != *hitNode) {
            if (event.mods.isShiftDown()) {
                if (onConnectNodes_) {
                    onConnectNodes_(*selectedNodeId_, *hitNode);
                }
            } else if (event.mods.isAltDown()) {
                if (onDisconnectNodes_) {
                    onDisconnectNodes_(*selectedNodeId_, *hitNode);
                }
            }
        }

        if (!event.mods.isShiftDown() && !event.mods.isAltDown()) {
            draggingNodeId_ = *hitNode;
            dragOffset_ = event.position - cachedPositions_[*hitNode];
        } else {
            draggingNodeId_.reset();
        }
        selectedNodeId_ = *hitNode;
        juce::Logger::writeToLog("NodeGraphComponent :: selected node -> " + juce::String(*selectedNodeId_));
        grabKeyboardFocus();
        if (onSelectionChanged_) {
            onSelectionChanged_(selectedNodeId_);
        }
    } else {
        draggingNodeId_.reset();
        if (!event.mods.isAnyModifierKeyDown()) {
            selectedNodeId_.reset();
            selectedConnection_.reset();
            if (onSelectionChanged_) {
                onSelectionChanged_(selectedNodeId_);
            }

            // Start panning when clicking on blank space
            if (viewport_ != nullptr) {
                isPanning_ = true;
                panStartViewportPos_ = viewport_->getViewPosition();
                panStartMousePos_ = event.getScreenPosition();
            }
        }
    }

    repaint();
}

void NodeGraphComponent::mouseDrag(const juce::MouseEvent& event) {
    if (draggingPort_) {
        dragPosition_ = event.position;
        hoverPort_ = findPortAt(event.position);
        if (hoverPort_ && (hoverPort_->nodeId == draggingPort_->nodeId || hoverPort_->isOutput == draggingPort_->isOutput)) {
            hoverPort_.reset();
        }
        repaint();
        return;
    }

    // Handle panning when dragging blank space
    if (isPanning_ && viewport_ != nullptr) {
        const auto currentMousePos = event.getScreenPosition();
        const auto delta = panStartMousePos_ - currentMousePos;
        viewport_->setViewPosition(panStartViewportPos_.x + delta.x, panStartViewportPos_.y + delta.y);
        return;
    }

    if (!draggingNodeId_ || view_ == nullptr) {
        return;
    }

    // Perform auto-scroll when dragging near viewport edges
    performAutoScroll(event.position);

    refreshDropTargets();

    auto center = event.position - dragOffset_;
    const auto constrainedArea = layoutArea_.reduced(kNodeWidth / 2.0F, kNodeHeight / 2.0F);
    if (constrainedArea.getWidth() > 0 && constrainedArea.getHeight() > 0) {
        center.x = juce::jlimit(constrainedArea.getX(), constrainedArea.getRight(), center.x);
        center.y = juce::jlimit(constrainedArea.getY(), constrainedArea.getBottom(), center.y);
    }

    const auto width = layoutArea_.getWidth();
    const auto height = layoutArea_.getHeight();
    if (width <= 0.0F || height <= 0.0F) {
        return;
    }

    const auto normalizedX = juce::jlimit(0.0F, 1.0F, (center.x - layoutArea_.getX()) / width);
    const auto normalizedY = juce::jlimit(0.0F, 1.0F, (center.y - layoutArea_.getY()) / height);

    auto normX = normOriginX_ + normalizedX * normSpanX_;
    auto normY = normOriginY_ + normalizedY * normSpanY_;
    normX = juce::jlimit(kNormMin, kNormMax, normX);
    normY = juce::jlimit(kNormMin, kNormMax, normY);

    view_->setPositionOverride(*draggingNodeId_, normX, normY);
    cachedPositions_[*draggingNodeId_] = center;
    if (onNodeDragged_) {
        onNodeDragged_(*draggingNodeId_, normX, normY);
    }

    pendingDropConnection_ = connectionNear(event.position);

    if (const auto target = hitTestNode(event.position)) {
        if (*target != *draggingNodeId_) {
            swapTargetId_ = *target;
        } else {
            swapTargetId_.reset();
        }
    } else {
        swapTargetId_.reset();
    }

    repaint();
}

void NodeGraphComponent::mouseUp(const juce::MouseEvent& event) {
    if (draggingPort_) {
        if (hoverPort_ && onPortConnected_) {
            const auto& start = *draggingPort_;
            const auto& target = *hoverPort_;
            if (start.isOutput != target.isOutput && start.nodeId != target.nodeId) {
                const auto& from = start.isOutput ? start : target;
                const auto& to = start.isOutput ? target : start;
                onPortConnected_(from.nodeId, from.index, to.nodeId, to.index);
            }
        }
        draggingPort_.reset();
        hoverPort_.reset();
        repaint();
        return;
    }

    if (draggingNodeId_) {
        refreshDropTargets();
        auto dropConnection = connectionNear(event.position);
        const auto nodeId = *draggingNodeId_;

        if (swapTargetId_ && *swapTargetId_ != nodeId && onNodesSwapped_) {
            onNodesSwapped_(nodeId, *swapTargetId_);
        } else if (dropConnection && onNodeInserted_ &&
            nodeId != dropConnection->first && nodeId != dropConnection->second) {
            onNodeInserted_(nodeId, *dropConnection);
        }

        draggingNodeId_.reset();
        swapTargetId_.reset();
        pendingDropConnection_.reset();
        repaint();
        return;
    }

    pendingDropConnection_.reset();
    swapTargetId_.reset();
    draggingNodeId_.reset();
    isPanning_ = false;
}

void NodeGraphComponent::mouseDoubleClick(const juce::MouseEvent& event) {
    if (view_ == nullptr) {
        return;
    }

    refreshCachedPositions();

    if (const auto hitNode = hitTestNode(event.position)) {
        if (const auto bounds = labelBoundsForNode(*hitNode); bounds && bounds->contains(event.position)) {
            beginInlineRename(*hitNode, bounds->toNearestInt());
            return;
        }

        if (onNodeDoubleClicked_) {
            onNodeDoubleClicked_(*hitNode);
        }
    }
}

bool NodeGraphComponent::keyPressed(const juce::KeyPress& key) {
    juce::Logger::writeToLog("NodeGraphComponent :: keyPressed code=" + juce::String(key.getKeyCode())
        + " cmd=" + juce::String(key.getModifiers().isCommandDown() ? "true" : "false")
        + " selection=" + (selectedNodeId_ ? *selectedNodeId_ : juce::String("<none>")));

    if (renameEditor_) {
        if (key.getKeyCode() == juce::KeyPress::escapeKey) {
            commitInlineRename(false);
            return true;
        }
        return false;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey) {
        if (selectedNodeId_) {
            selectedNodeId_.reset();
            if (onSelectionChanged_) {
                onSelectionChanged_(selectedNodeId_);
            }
            repaint();
            return true;
        }
        if (selectedConnection_) {
            selectedConnection_.reset();
            repaint();
            return true;
        }
    }

    if ((key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey) && selectedConnection_) {
        if (onDisconnectNodes_) {
            onDisconnectNodes_(selectedConnection_->first, selectedConnection_->second);
        }
        selectedConnection_.reset();
        repaint();
        return true;
    }
    return false;
}

void NodeGraphComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    if (viewport_ == nullptr) {
        return;
    }

    // Calculate scroll delta based on wheel movement
    // Higher multiplier for faster trackpad scrolling
    constexpr float kScrollMultiplier = 200.0F;
    const int deltaX = static_cast<int>(-wheel.deltaX * kScrollMultiplier);
    const int deltaY = static_cast<int>(-wheel.deltaY * kScrollMultiplier);

    // Get current viewport position
    auto currentPos = viewport_->getViewPosition();

    // Apply the delta
    currentPos.x += deltaX;
    currentPos.y += deltaY;

    // Set the new viewport position
    viewport_->setViewPosition(currentPos);

    // Suppress default behavior
    event.source.enableUnboundedMouseMovement(false);
}

void NodeGraphComponent::mouseMagnify(const juce::MouseEvent& event, float scaleFactor) {
    // Handle trackpad pinch-to-zoom gesture
    constexpr float kMinZoom = 0.5F;
    constexpr float kMaxZoom = 3.0F;

    // Apply scale factor to current zoom level
    const float newZoom = juce::jlimit(kMinZoom, kMaxZoom, zoomLevel_ * scaleFactor);

    if (std::abs(newZoom - zoomLevel_) > 0.001F) {
        // Get mouse position in component coordinates before zoom
        const auto mousePos = event.position;

        // Update zoom level
        zoomLevel_ = newZoom;

        // Apply transform for zoom
        setTransform(juce::AffineTransform::scale(zoomLevel_));

        // Force refresh of cached positions
        refreshCachedPositions(true);
        repaint();
    }
}

void NodeGraphComponent::refreshDropTargets() {
    if (view_ == nullptr) {
        return;
    }

    refreshCachedPositions();
    resolveFixedEndpoints();
    inputPortPositions_.clear();
    outputPortPositions_.clear();
    fixedInputAnchor_.reset();
    fixedOutputAnchor_.reset();

    const auto computePortY = [](const juce::Rectangle<float>& bounds, std::uint32_t count, std::size_t index) {
        if (count == 0) {
            return bounds.getCentreY();
        }
        const auto fraction = (static_cast<float>(index) + 1.0F) / (static_cast<float>(count) + 1.0F);
        return bounds.getY() + fraction * bounds.getHeight();
    };

    for (const auto& nodeVisual : view_->nodes()) {
        const auto posIt = cachedPositions_.find(nodeVisual.id);
        if (posIt == cachedPositions_.end()) {
            continue;
        }

        const auto position = posIt->second;
        const bool isFixedInput = fixedInputEnabled_ &&
            ((fixedInputId_ && nodeVisual.id == *fixedInputId_) || nodeVisual.type == audio::GraphNodeType::Input);
        const bool isFixedOutput = fixedOutputEnabled_ &&
            ((fixedOutputId_ && nodeVisual.id == *fixedOutputId_) || nodeVisual.type == audio::GraphNodeType::Output);

        if (isFixedInput || isFixedOutput) {
            const auto anchorX = isFixedInput ? layoutArea_.getX() + 10.0F : layoutArea_.getRight() - 10.0F;
            const auto anchorY = juce::jlimit(layoutArea_.getY() + 12.0F, layoutArea_.getBottom() - 12.0F, position.y);
            const auto anchor = juce::Point<float>(anchorX, anchorY);
            if (isFixedInput) {
                fixedInputId_ = nodeVisual.id;
                fixedInputAnchor_ = anchor;
                inputPortPositions_[nodeVisual.id] = {};
                outputPortPositions_[nodeVisual.id] = { anchor };
            } else {
                fixedOutputId_ = nodeVisual.id;
                fixedOutputAnchor_ = anchor;
                inputPortPositions_[nodeVisual.id] = { anchor };
                outputPortPositions_[nodeVisual.id] = {};
            }
            continue;
        }

        const auto nodeBounds = nodeBoundsForPosition(position);

        std::vector<juce::Point<float>> inputPorts;
        const auto inputPortCount = nodeVisual.inputChannels > 0 ? 1U : 0U;
        for (std::size_t i = 0; i < static_cast<std::size_t>(inputPortCount); ++i) {
            const auto portPoint = juce::Point<float>(
                nodeBounds.getX(),
                computePortY(nodeBounds, inputPortCount, i));
            inputPorts.push_back(portPoint);
        }
        inputPortPositions_[nodeVisual.id] = std::move(inputPorts);

        std::vector<juce::Point<float>> outputPorts;
        const auto outputPortCount = nodeVisual.outputChannels > 0 ? 1U : 0U;
        for (std::size_t i = 0; i < static_cast<std::size_t>(outputPortCount); ++i) {
            const auto portPoint = juce::Point<float>(
                nodeBounds.getRight(),
                computePortY(nodeBounds, outputPortCount, i));
            outputPorts.push_back(portPoint);
        }
        outputPortPositions_[nodeVisual.id] = std::move(outputPorts);
    }

    connectionSegments_.clear();
    std::unordered_set<std::string> drawnConnections;
    drawnConnections.reserve(view_->connections().size());
    for (const auto& connection : view_->connections()) {
        const auto key = connection.fromId + "->" + connection.toId;
        if (!drawnConnections.insert(key).second) {
            continue;
        }

        const auto fromPoint = portPosition(PortSelection { connection.fromId, true, connection.fromPort });
        const auto toPoint = portPosition(PortSelection { connection.toId, false, connection.toPort });
        connectionSegments_.push_back(ConnectionSegment { connection.fromId, connection.toId, juce::Line<float>(fromPoint, toPoint) });
    }
}

void NodeGraphComponent::performAutoScroll(const juce::Point<float>& mousePosition) {
    if (viewport_ == nullptr) {
        return;
    }

    // Define auto-scroll edge zone (pixels from edge)
    constexpr int kAutoScrollZone = 50;
    constexpr int kAutoScrollSpeed = 15;

    // Get viewport bounds relative to this component
    const auto viewportBounds = getLocalBounds();
    const auto viewPos = viewport_->getViewPosition();

    int scrollDeltaX = 0;
    int scrollDeltaY = 0;

    // Check if mouse is near left edge
    if (mousePosition.x < kAutoScrollZone) {
        scrollDeltaX = -kAutoScrollSpeed;
    }
    // Check if mouse is near right edge
    else if (mousePosition.x > viewportBounds.getWidth() - kAutoScrollZone) {
        scrollDeltaX = kAutoScrollSpeed;
    }

    // Check if mouse is near top edge
    if (mousePosition.y < kAutoScrollZone) {
        scrollDeltaY = -kAutoScrollSpeed;
    }
    // Check if mouse is near bottom edge
    else if (mousePosition.y > viewportBounds.getHeight() - kAutoScrollZone) {
        scrollDeltaY = kAutoScrollSpeed;
    }

    // Apply auto-scroll if needed
    if (scrollDeltaX != 0 || scrollDeltaY != 0) {
        viewport_->setViewPosition(viewPos.x + scrollDeltaX, viewPos.y + scrollDeltaY);
    }
}

std::optional<std::pair<std::string, std::string>> NodeGraphComponent::connectionNear(const juce::Point<float>& position) const {
    if (connectionSegments_.empty()) {
        return std::nullopt;
    }

    std::optional<std::pair<std::string, std::string>> closest;
    float bestDistance = kConnectionDropTolerance;

    for (const auto& segment : connectionSegments_) {
        juce::Point<float> closestPoint;
        const auto distance = segment.line.getDistanceFromPoint(position, closestPoint);
        if (distance <= bestDistance) {
            bestDistance = distance;
            closest = std::make_pair(segment.fromId, segment.toId);
        }
    }
    return closest;
}

bool NodeGraphComponent::isInterestedInDragSource(const SourceDetails& dragSourceDetails) {
    return dragSourceDetails.description.isString();
}

void NodeGraphComponent::itemDragEnter(const SourceDetails& dragSourceDetails) {
    if (!dragSourceDetails.description.isString()) {
        return;
    }

    refreshDropTargets();
    pendingDropType_ = dragSourceDetails.description.toString().toStdString();
    auto position = dragSourceDetails.localPosition.toFloat();
    position.x = juce::jlimit(layoutArea_.getX(), layoutArea_.getRight(), position.x);
    position.y = juce::jlimit(layoutArea_.getY(), layoutArea_.getBottom(), position.y);
    pendingDropPosition_ = position;
    pendingDropConnection_ = connectionNear(position);
    repaint();
}

void NodeGraphComponent::itemDragMove(const SourceDetails& dragSourceDetails) {
    if (!dragSourceDetails.description.isString()) {
        return;
    }

    refreshDropTargets();
    pendingDropType_ = dragSourceDetails.description.toString().toStdString();
    auto position = dragSourceDetails.localPosition.toFloat();
    position.x = juce::jlimit(layoutArea_.getX(), layoutArea_.getRight(), position.x);
    position.y = juce::jlimit(layoutArea_.getY(), layoutArea_.getBottom(), position.y);
    pendingDropPosition_ = position;
    pendingDropConnection_ = connectionNear(position);
    repaint();
}

void NodeGraphComponent::itemDragExit(const SourceDetails&) {
    pendingDropType_.reset();
    pendingDropPosition_.reset();
    pendingDropConnection_.reset();
    repaint();
}

void NodeGraphComponent::itemDropped(const SourceDetails& dragSourceDetails) {
    auto dropType = pendingDropType_;
    auto dropPosition = pendingDropPosition_;
    auto dropConnection = pendingDropConnection_;

    pendingDropType_.reset();
    pendingDropPosition_.reset();
    pendingDropConnection_.reset();
    repaint();

    if (!dragSourceDetails.description.isString() || !onNodeCreated_ || view_ == nullptr || !dropType || !dropPosition) {
        return;
    }

    refreshDropTargets();

    const auto width = layoutArea_.getWidth();
    const auto height = layoutArea_.getHeight();
    if (width <= 0.0F || height <= 0.0F) {
        return;
    }

    auto position = *dropPosition;
    position.x = juce::jlimit(layoutArea_.getX(), layoutArea_.getRight(), position.x);
    position.y = juce::jlimit(layoutArea_.getY(), layoutArea_.getBottom(), position.y);

    auto normalizedX = juce::jlimit(0.0F, 1.0F, (position.x - layoutArea_.getX()) / width);
    auto normalizedY = juce::jlimit(0.0F, 1.0F, (position.y - layoutArea_.getY()) / height);
    auto normX = juce::jlimit(kNormMin, kNormMax, normOriginX_ + normalizedX * normSpanX_);
    auto normY = juce::jlimit(kNormMin, kNormMax, normOriginY_ + normalizedY * normSpanY_);

    NodeCreateRequest request;
    request.templateId = *dropType;
    request.normX = normX;
    request.normY = normY;
    if (auto nearbyConnection = connectionNear(position)) {
        request.insertBetween = nearbyConnection;
    } else if (dropConnection) {
        request.insertBetween = dropConnection;
    }

    onNodeCreated_(request);
}

void NodeGraphComponent::setNodeDoubleClickHandler(std::function<void(const std::string&)> handler) {
    onNodeDoubleClicked_ = std::move(handler);
}

void NodeGraphComponent::setNodeDragHandler(std::function<void(const std::string&, float, float)> handler) {
    onNodeDragged_ = std::move(handler);
}

void NodeGraphComponent::setMeterProvider(std::function<std::array<float, 2>(const std::string&)> provider) {
    meterProvider_ = std::move(provider);
}

void NodeGraphComponent::setGraphView(ui::NodeGraphView* view) {
    commitInlineRename(false);
    view_ = view;
    lastLayoutVersion_ = 0;
    draggingNodeId_.reset();
    selectedNodeId_.reset();
    draggingPort_.reset();
    hoverPort_.reset();
    cachedPositions_.clear();
    cachedPositionsVersion_ = std::numeric_limits<std::size_t>::max();
    lastWidth_ = -1;
    lastHeight_ = -1;
    lastContentWidth_ = -1;
    lastContentHeight_ = -1;
    normOriginX_ = 0.0F;
    normOriginY_ = 0.0F;
    normSpanX_ = 1.0F;
    normSpanY_ = 1.0F;
    labelBoundsCache_.clear();
    avatarCache_.clear();
    inputPortPositions_.clear();
    outputPortPositions_.clear();
    connectionSegments_.clear();
    selectedConnection_.reset();
    pendingDropType_.reset();
    pendingDropPosition_.reset();
    pendingDropConnection_.reset();
    swapTargetId_.reset();
    fixedInputAnchor_.reset();
    fixedOutputAnchor_.reset();
    fixedInputNormY_.reset();
    fixedOutputNormY_.reset();
    fixedInputEnabled_ = false;
    fixedOutputEnabled_ = false;
    refreshCachedPositions(true);
    if (onSelectionChanged_) {
        onSelectionChanged_(selectedNodeId_);
    }
    repaint();
}

void NodeGraphComponent::setConnectNodesHandler(std::function<void(const std::string&, const std::string&)> handler) {
    onConnectNodes_ = std::move(handler);
}

void NodeGraphComponent::setDisconnectNodesHandler(std::function<void(const std::string&, const std::string&)> handler) {
    onDisconnectNodes_ = std::move(handler);
}

void NodeGraphComponent::setSelectionChangedHandler(std::function<void(const std::optional<std::string>&)> handler) {
    onSelectionChanged_ = std::move(handler);
}

void NodeGraphComponent::setPortConnectHandler(std::function<void(const std::string&, std::size_t, const std::string&, std::size_t)> handler) {
    onPortConnected_ = std::move(handler);
}

void NodeGraphComponent::setNodeCreateHandler(std::function<void(const NodeCreateRequest&)> handler) {
    onNodeCreated_ = std::move(handler);
}

void NodeGraphComponent::setNodeSwapHandler(std::function<void(const std::string&, const std::string&)> handler) {
    onNodesSwapped_ = std::move(handler);
}

void NodeGraphComponent::setNodeInsertHandler(std::function<void(const std::string&, const std::pair<std::string, std::string>&)> handler) {
    onNodeInserted_ = std::move(handler);
}

void NodeGraphComponent::setFixedEndpoints(std::optional<std::string> inputId, std::optional<std::string> outputId) {
    fixedInputId_ = std::move(inputId);
    fixedOutputId_ = std::move(outputId);
    fixedInputAnchor_.reset();
    fixedOutputAnchor_.reset();
    fixedInputNormY_.reset();
    fixedOutputNormY_.reset();
    fixedInputEnabled_ = fixedInputId_.has_value();
    fixedOutputEnabled_ = fixedOutputId_.has_value();
    resolveFixedEndpoints();
    repaint();
}

void NodeGraphComponent::resolveFixedEndpoints() {
    if (view_ == nullptr) {
        return;
    }

    const auto resolve = [&](std::optional<std::string>& id,
                             std::optional<float>& storedNormY,
                             bool enabled,
                             audio::GraphNodeType desiredType,
                             float normX) {
        if (!enabled) {
            id.reset();
            storedNormY.reset();
            return;
        }

        const auto hasId = [&]() {
            if (!id) {
                return false;
            }
            return std::any_of(view_->nodes().begin(), view_->nodes().end(), [&](const auto& node) {
                return node.id == *id;
            });
        }();

        if (!hasId) {
            for (const auto& node : view_->nodes()) {
                if (node.type == desiredType) {
                    id = node.id;
                    break;
                }
            }
        }

        if (id) {
            auto currentNode = std::find_if(view_->nodes().begin(), view_->nodes().end(), [&](const auto& node) {
                return node.id == *id;
            });
            if (currentNode != view_->nodes().end()) {
                const float desiredNormY = storedNormY.value_or(currentNode->normY);
                if (std::abs(currentNode->normX - normX) > 0.0001F || std::abs(currentNode->normY - desiredNormY) > 0.0001F) {
                    view_->setPositionOverride(*id, normX, desiredNormY);
                }
                storedNormY = desiredNormY;
            }
        }
    };

    resolve(fixedInputId_, fixedInputNormY_, fixedInputEnabled_, audio::GraphNodeType::Input, 0.02F);
    resolve(fixedOutputId_, fixedOutputNormY_, fixedOutputEnabled_, audio::GraphNodeType::Output, 0.98F);
}

std::optional<std::string> NodeGraphComponent::selectedNode() const noexcept {
    return selectedNodeId_;
}

juce::Colour NodeGraphComponent::toColour(const ui::Color& color) const {
    return juce::Colour::fromFloatRGBA(color.r, color.g, color.b, color.a);
}

juce::Colour NodeGraphComponent::nodeFillColour(audio::GraphNodeType type) const {
    const auto base = view_ ? toColour(view_->theme().accent) : juce::Colour::fromFloatRGBA(0.33F, 0.46F, 0.66F, 1.0F);
    switch (type) {
    case audio::GraphNodeType::Input:
        return base.withMultipliedBrightness(1.2F);
    case audio::GraphNodeType::Channel:
        return base;
    case audio::GraphNodeType::GroupBus:
        return base.darker(0.1F);
    case audio::GraphNodeType::Person:
        return base.darker(0.05F);
    case audio::GraphNodeType::BroadcastBus:
        return base.darker(0.25F);
    case audio::GraphNodeType::MixBus:
        return base.darker(0.35F);
    case audio::GraphNodeType::Utility:
        return base.withMultipliedSaturation(0.6F).brighter(0.1F);
    case audio::GraphNodeType::Plugin:
        return base.withHue(base.getHue() + 0.08F);
    case audio::GraphNodeType::SignalGenerator:
        return base.withSaturation(base.getSaturation() * 0.6F).withHue(base.getHue() - 0.05F).brighter(0.2F);
    case audio::GraphNodeType::Output:
        return base.withMultipliedBrightness(1.4F);
    default:
        return base;
    }
}

void NodeGraphComponent::refreshCachedPositions(bool force) {
    // Prevent recursive calls that could cause infinite loops
    if (isRefreshingPositions_) {
        return;
    }
    isRefreshingPositions_ = true;

    auto bounds = getLocalBounds();

    if (view_ == nullptr) {
        isRefreshingPositions_ = false;
        layoutArea_ = computeLayoutArea();
        cachedPositions_.clear();
        cachedPositionsVersion_ = std::numeric_limits<std::size_t>::max();
        lastWidth_ = bounds.getWidth();
        lastHeight_ = bounds.getHeight();
        lastContentWidth_ = bounds.getWidth();
        lastContentHeight_ = bounds.getHeight();
        normOriginX_ = 0.0F;
        normOriginY_ = 0.0F;
        normSpanX_ = 1.0F;
        normSpanY_ = 1.0F;
        return;
    }

    const bool useMicroCanvas = fixedInputEnabled_ || fixedOutputEnabled_;

    float minNormX = std::numeric_limits<float>::max();
    float maxNormX = std::numeric_limits<float>::lowest();
    float minNormY = std::numeric_limits<float>::max();
    float maxNormY = std::numeric_limits<float>::lowest();

    for (const auto& node : view_->nodes()) {
        minNormX = std::min(minNormX, node.normX);
        maxNormX = std::max(maxNormX, node.normX);
        minNormY = std::min(minNormY, node.normY);
        maxNormY = std::max(maxNormY, node.normY);
    }

    if (!view_->nodes().empty()) {
        if (useMicroCanvas) {
            minNormX = std::min(minNormX, kMicroNormMin);
            maxNormX = std::max(maxNormX, kMicroNormMax);
            minNormY = std::min(minNormY, kMicroNormMinY);
            maxNormY = std::max(maxNormY, kMicroNormMaxY);
        } else {
            minNormX = std::min(minNormX, kNormMin);
            maxNormX = std::max(maxNormX, kNormMax - 0.25F);
            minNormY = std::min(minNormY, kNormMin);
            maxNormY = std::max(maxNormY, kNormMax - 0.25F);
        }
    } else {
        minNormX = 0.0F;
        maxNormX = 1.0F;
        minNormY = 0.0F;
        maxNormY = 1.0F;
    }

    const float paddedMinX = minNormX - kNormPadding;
    const float paddedMaxX = maxNormX + kNormPadding;
    const float paddedMinY = minNormY - kNormPadding;
    const float paddedMaxY = maxNormY + kNormPadding;

    normOriginX_ = paddedMinX;
    normOriginY_ = paddedMinY;
    normSpanX_ = std::max(0.001F, paddedMaxX - paddedMinX);
    normSpanY_ = std::max(0.001F, paddedMaxY - paddedMinY);

    // Calculate canvas size based on normalized coordinate range
    // Each normalized unit gets 600 pixels of space (enough for ~4 nodes horizontally)
    constexpr float kPixelsPerNormUnit = 600.0F;
    const int derivedWidth = static_cast<int>(std::ceil(normSpanX_ * kPixelsPerNormUnit));
    const int derivedHeight = static_cast<int>(std::ceil(normSpanY_ * kPixelsPerNormUnit));

    const int minCanvasWidth = useMicroCanvas ? kMinMicroCanvasWidth : kMinMacroCanvasWidth;
    const int minCanvasHeight = useMicroCanvas ? kMinMicroCanvasHeight : kMinMacroCanvasHeight;

    // Canvas must be at least as large as viewport, and large enough for scrolling
    const int contentWidth = std::max({bounds.getWidth(), derivedWidth, minCanvasWidth});
    const int contentHeight = std::max({bounds.getHeight(), derivedHeight, minCanvasHeight});

    if (contentWidth != getWidth() || contentHeight != getHeight()) {
        lastContentWidth_ = contentWidth;
        lastContentHeight_ = contentHeight;
        setSize(contentWidth, contentHeight);
        bounds = getLocalBounds();
    } else {
        lastContentWidth_ = contentWidth;
        lastContentHeight_ = contentHeight;
    }

    const auto layoutVersion = view_->layoutVersion();
    const bool sizeChanged = bounds.getWidth() != lastWidth_ || bounds.getHeight() != lastHeight_;
    if (!force && !sizeChanged && layoutVersion == cachedPositionsVersion_) {
        isRefreshingPositions_ = false;
        return;
    }

    lastWidth_ = bounds.getWidth();
    lastHeight_ = bounds.getHeight();
    cachedPositionsVersion_ = layoutVersion;

    layoutArea_ = computeLayoutArea();
    cachedPositions_.clear();
    cachedPositions_.reserve(view_->nodes().size());

    for (const auto& node : view_->nodes()) {
        const auto normPos = juce::Point<float>(node.normX, node.normY);
        const float normalizedX = (normPos.x - normOriginX_) / normSpanX_;
        const float normalizedY = (normPos.y - normOriginY_) / normSpanY_;
        const auto centerX = layoutArea_.getX() + juce::jlimit(0.0F, 1.0F, normalizedX) * layoutArea_.getWidth();
        const auto centerY = layoutArea_.getY() + juce::jlimit(0.0F, 1.0F, normalizedY) * layoutArea_.getHeight();
        cachedPositions_.emplace(node.id, juce::Point<float>(centerX, centerY));
    }

    isRefreshingPositions_ = false;
}

juce::Rectangle<float> NodeGraphComponent::computeLayoutArea() const {
    auto bounds = getLocalBounds().toFloat();
    bounds.reduce(kHorizontalPadding, kVerticalPadding);
    if (bounds.getWidth() < kNodeWidth || bounds.getHeight() < kNodeHeight) {
        return getLocalBounds().toFloat();
    }
    return bounds;
}

juce::Rectangle<float> NodeGraphComponent::nodeBoundsForPosition(const juce::Point<float>& position) const {
    return {
        position.x - (kNodeWidth / 2.0F),
        position.y - (kNodeHeight / 2.0F),
        kNodeWidth,
        kNodeHeight
    };
}

std::optional<std::string> NodeGraphComponent::hitTestNode(const juce::Point<float>& position) const {
    if (view_ == nullptr) {
        return std::nullopt;
    }

    for (const auto& nodePos : cachedPositions_) {
        if ((fixedInputEnabled_ && fixedInputId_ && nodePos.first == *fixedInputId_) ||
            (fixedOutputEnabled_ && fixedOutputId_ && nodePos.first == *fixedOutputId_)) {
            continue;
        }
        if (nodeBoundsForPosition(nodePos.second).contains(position)) {
            return nodePos.first;
        }
    }
    return std::nullopt;
}

std::optional<NodeGraphComponent::PortSelection> NodeGraphComponent::findPortAt(const juce::Point<float>& position) const {
    const auto checkMap = [&](const auto& map, bool isOutput) -> std::optional<PortSelection> {
        for (const auto& entry : map) {
            const auto& ports = entry.second;
            for (std::size_t index = 0; index < ports.size(); ++index) {
                if (ports[index].getDistanceFrom(position) <= kPortHitRadius) {
                    return PortSelection { entry.first, isOutput, index };
                }
            }
        }
        return std::nullopt;
    };

    if (auto port = checkMap(outputPortPositions_, true)) {
        return port;
    }
    return checkMap(inputPortPositions_, false);
}

juce::Point<float> NodeGraphComponent::portPosition(const PortSelection& port) const {
    const auto& map = port.isOutput ? outputPortPositions_ : inputPortPositions_;
    if (const auto it = map.find(port.nodeId); it != map.end()) {
        if (port.index < it->second.size()) {
            return it->second[port.index];
        }
    }

    if (fixedInputEnabled_ && fixedInputId_ && port.nodeId == *fixedInputId_ && fixedInputAnchor_) {
        return *fixedInputAnchor_;
    }
    if (fixedOutputEnabled_ && fixedOutputId_ && port.nodeId == *fixedOutputId_ && fixedOutputAnchor_) {
        return *fixedOutputAnchor_;
    }

    if (const auto nodeIt = cachedPositions_.find(port.nodeId); nodeIt != cachedPositions_.end()) {
        return nodeIt->second;
    }

    return juce::Point<float> {};
}

std::optional<juce::Rectangle<float>> NodeGraphComponent::labelBoundsForNode(const std::string& nodeId) const {
    if (const auto it = labelBoundsCache_.find(nodeId); it != labelBoundsCache_.end()) {
        return it->second.bounds;
    }
    if (view_ == nullptr) {
        return std::nullopt;
    }
    const auto posIt = cachedPositions_.find(nodeId);
    if (posIt == cachedPositions_.end()) {
        return std::nullopt;
    }
    const auto nodeBounds = nodeBoundsForPosition(posIt->second);
    return nodeBounds.reduced(12.0F);
}

juce::Rectangle<float> NodeGraphComponent::labelBoundsForText(const std::string& nodeId,
                                                              const std::string& text,
                                                              bool isPerson,
                                                              const juce::Font& font,
                                                              const juce::Rectangle<float>& available,
                                                              juce::Justification justification) {
    auto& entry = labelBoundsCache_[nodeId];
    if (entry.text == text && entry.isPerson == isPerson && entry.availableBounds == available && !entry.bounds.isEmpty()) {
        return entry.bounds;
    }

    juce::GlyphArrangement arrangement;
    arrangement.addFittedText(font,
                              juce::String(text),
                              available.getX(),
                              available.getY(),
                              available.getWidth(),
                              available.getHeight(),
                              justification,
                              1);
    auto bounds = arrangement.getBoundingBox(0, arrangement.getNumGlyphs(), true);
    if (bounds.isEmpty()) {
        bounds = available;
    }

    entry.bounds = bounds;
    entry.availableBounds = available;
    entry.text = text;
    entry.isPerson = isPerson;
    return entry.bounds;
}

juce::Image NodeGraphComponent::cachedAvatarForPath(const std::string& path) {
    if (path.empty()) {
        return {};
    }

    if (const auto it = avatarCache_.find(path); it != avatarCache_.end()) {
        return it->second;
    }

    auto image = loadAvatarImage(path);
    if (image.isValid()) {
        avatarCache_.emplace(path, image);
    }
    return image;
}

void NodeGraphComponent::beginInlineRename(const std::string& nodeId, const juce::Rectangle<int>& bounds) {
    if (view_ == nullptr) {
        return;
    }
    if (renameEditor_) {
        commitInlineRename(false);
    }

    std::string currentLabel = nodeId;
    const auto& nodes = view_->nodes();
    const auto nodeIt = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
        return node.id == nodeId;
    });
    if (nodeIt != nodes.end() && !nodeIt->label.empty()) {
        currentLabel = nodeIt->label;
    }

    auto editor = std::make_unique<juce::TextEditor>();
    editor->setBounds(bounds);
    editor->setIndents(0, 0);
    editor->setBorder(juce::BorderSize<int>());
    editor->setReturnKeyStartsNewLine(false);
    editor->setJustification(juce::Justification::centred);
    editor->setOpaque(false);
    editor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    editor->setColour(juce::TextEditor::textColourId, toColour(view_->theme().textPrimary));
    editor->setColour(juce::TextEditor::highlightColourId, toColour(view_->theme().accent).withAlpha(0.2F));
    editor->setColour(juce::TextEditor::highlightedTextColourId, toColour(view_->theme().textPrimary));
    editor->setPopupMenuEnabled(false);
    editor->setScrollbarsShown(false);
    editor->setFont(juce::Font(juce::FontOptions { 15.0F, juce::Font::bold }));
    editor->setSelectAllWhenFocused(true);
    editor->setText(currentLabel, juce::dontSendNotification);
    editor->addListener(this);

    renamingNodeId_ = nodeId;
    renameOriginalText_ = currentLabel;
    renameEditor_ = std::move(editor);
    addAndMakeVisible(renameEditor_.get());
    renameEditor_->grabKeyboardFocus();
    renameEditor_->selectAll();
}

void NodeGraphComponent::commitInlineRename(bool apply) {
    if (!renameEditor_) {
        return;
    }

    auto* editor = renameEditor_.get();
    const auto nodeId = renamingNodeId_;
    const auto original = renameOriginalText_;

    editor->removeListener(this);
    removeChildComponent(editor);

    std::string newLabel;
    if (apply) {
        auto text = editor->getText().trim();
        newLabel = text.toStdString();
    }

    renameEditor_.reset();
    renamingNodeId_.clear();
    renameOriginalText_.clear();

    if (apply && onNodeRenamed_ && !nodeId.empty()) {
        if (newLabel != original) {
            onNodeRenamed_(nodeId, newLabel);
        }
    }

    repaint();
}

void NodeGraphComponent::setNodeRenameHandler(std::function<void(const std::string&, const std::string&)> handler) {
    onNodeRenamed_ = std::move(handler);
}

void NodeGraphComponent::beginNodeRename(const std::string& nodeId) {
    refreshCachedPositions();
    if (const auto bounds = labelBoundsForNode(nodeId)) {
        beginInlineRename(nodeId, bounds->toNearestInt());
    }
}

void NodeGraphComponent::setViewport(juce::Viewport* viewport) {
    viewport_ = viewport;
}

void NodeGraphComponent::resetZoom() {
    if (std::abs(zoomLevel_ - 1.0F) > 0.001F) {
        zoomLevel_ = 1.0F;
        setTransform(juce::AffineTransform());
        refreshCachedPositions(true);
        repaint();
    }
}

void NodeGraphComponent::setZoom(float zoom) {
    constexpr float kMinZoom = 0.5F;
    constexpr float kMaxZoom = 3.0F;
    const float clampedZoom = juce::jlimit(kMinZoom, kMaxZoom, zoom);

    juce::Logger::writeToLog("setZoom called: requested=" + juce::String(zoom) +
                             " clamped=" + juce::String(clampedZoom) +
                             " current=" + juce::String(zoomLevel_));

    if (std::abs(zoomLevel_ - clampedZoom) > 0.001F) {
        zoomLevel_ = clampedZoom;
        setTransform(juce::AffineTransform::scale(zoomLevel_));
        refreshCachedPositions(true);
        repaint();
        juce::Logger::writeToLog("Zoom applied: " + juce::String(zoomLevel_));
    } else {
        juce::Logger::writeToLog("Zoom not changed (too small difference)");
    }
}

void NodeGraphComponent::focusNodes(const std::vector<std::string>& nodeIds,
                                    FocusAlignment alignment,
                                    bool fitToViewport,
                                    int retryCount) {
    if (viewport_ == nullptr) {
        juce::Logger::writeToLog("focusNodes aborted: viewport null");
        return;
    }

    refreshCachedPositions(true);

    juce::Rectangle<float> targetBounds;
    bool hasBounds = false;

    const auto accumulateNode = [&](const std::string& nodeId) {
        const auto posIt = cachedPositions_.find(nodeId);
        if (posIt == cachedPositions_.end()) {
            return;
        }
        const auto bounds = nodeBoundsForPosition(posIt->second);
        if (!hasBounds) {
            targetBounds = bounds;
            hasBounds = true;
        } else {
            targetBounds = targetBounds.getUnion(bounds);
        }
    };

    if (!nodeIds.empty()) {
        for (const auto& id : nodeIds) {
            accumulateNode(id);
        }
    }

    if (!hasBounds) {
        for (const auto& entry : cachedPositions_) {
            accumulateNode(entry.first);
        }
    }

    if (!hasBounds) {
        juce::Logger::writeToLog("focusNodes aborted: no node bounds");
        return;
    }

    targetBounds.expand(40.0F, 40.0F);

    const int viewWidth = viewport_->getViewWidth();
    const int viewHeight = viewport_->getViewHeight();
    if (viewWidth <= 0 || viewHeight <= 0) {
        juce::Logger::writeToLog("focusNodes aborted: viewport dimensions invalid");
        if (retryCount < 5) {
            juce::Component::SafePointer<NodeGraphComponent> safeThis(this);
            auto retryIds = nodeIds;
            juce::MessageManager::callAsync([safeThis, ids = std::move(retryIds), alignment, fitToViewport, retryCount]() mutable {
                if (safeThis == nullptr) {
                    return;
                }
                safeThis->focusNodes(ids, alignment, fitToViewport, retryCount + 1);
            });
        }
        return;
    }

    const float currentZoom = zoomLevel_;
    float targetZoom = currentZoom;

    if (fitToViewport) {
        const float widthZoom = targetBounds.getWidth() > 0.0F
            ? static_cast<float>(viewWidth) / targetBounds.getWidth()
            : targetZoom;
        const float heightZoom = targetBounds.getHeight() > 0.0F
            ? static_cast<float>(viewHeight) / targetBounds.getHeight()
            : targetZoom;
        const float desired = 0.92F * std::min(widthZoom, heightZoom);
        targetZoom = juce::jlimit(0.4F, 1.0F, desired);
    }

    if (std::abs(targetZoom - currentZoom) > 0.001F) {
        setZoom(targetZoom);
    }

    const float appliedZoom = zoomLevel_;
    const float effectiveViewWidth = static_cast<float>(viewWidth) / appliedZoom;
    const float effectiveViewHeight = static_cast<float>(viewHeight) / appliedZoom;

    float targetX = 0.0F;
    if (alignment == FocusAlignment::Right) {
        targetX = targetBounds.getRight() - (effectiveViewWidth - 60.0F);
    } else {
        targetX = targetBounds.getCentreX() - (effectiveViewWidth * 0.5F);
    }

    float targetY = targetBounds.getCentreY() - (effectiveViewHeight * 0.5F);

    const int maxScrollX = std::max(0, contentWidth() - viewWidth);
    const int maxScrollY = std::max(0, contentHeight() - viewHeight);

    const int clampedX = juce::jlimit(0, maxScrollX, static_cast<int>(std::round(targetX)));
    const int clampedY = juce::jlimit(0, maxScrollY, static_cast<int>(std::round(targetY)));

    viewport_->setViewPosition(clampedX, clampedY);
}

void NodeGraphComponent::textEditorReturnKeyPressed(juce::TextEditor& editor) {
    if (renameEditor_.get() == &editor) {
        commitInlineRename(true);
    }
}

void NodeGraphComponent::textEditorEscapeKeyPressed(juce::TextEditor& editor) {
    if (renameEditor_.get() == &editor) {
        commitInlineRename(false);
    }
}

void NodeGraphComponent::textEditorFocusLost(juce::TextEditor& editor) {
    if (renameEditor_.get() == &editor) {
        commitInlineRename(true);
    }
}

} // namespace broadcastmix::app
