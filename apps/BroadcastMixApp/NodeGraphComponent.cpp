#include "NodeGraphComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
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
    g.fillAll(toColour(theme.background));

    // Draw grid backdrop.
    g.setColour(toColour(theme.background).brighter(0.08F));
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
        g.setColour(toColour(theme.textPrimary).withAlpha(0.55F));
        g.setFont(juce::Font(juce::FontOptions { 16.0F }));
        g.drawFittedText("Drag nodes from the library",
                         area.toNearestInt(),
                         juce::Justification::centred,
                         1);
    }

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
            g.setColour(toColour(theme.accent).withAlpha(0.38F));
            g.fillEllipse(circleBounds);
            g.setColour(toColour(theme.accent));
            g.drawEllipse(circleBounds, 1.6F);

            g.setColour(toColour(theme.textPrimary));
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
        auto fillColour = nodeFillColour(nodeVisual.type);
        if (!nodeVisual.enabled) {
            fillColour = fillColour.withAlpha(0.35F);
        }

        juce::DropShadow shadow(toColour(theme.background).darker(0.4F), 10, {});
        shadow.drawForRectangle(g, nodeBounds.toNearestInt());

        g.setColour(fillColour);
        g.fillRoundedRectangle(nodeBounds, kCornerRadius);

        g.setColour(toColour(theme.textPrimary));
        auto labelBounds = nodeBounds.reduced(12.0F);
        if (renameEditor_ && renamingNodeId_ == nodeVisual.id) {
            renameEditor_->setBounds(labelBounds.toNearestInt());
        }
        g.setFont(juce::Font(juce::FontOptions { 15.0F, juce::Font::bold }));
        g.drawFittedText(nodeVisual.label,
                         labelBounds.toNearestInt(),
                         juce::Justification::centred,
                         1);

        if (selectedNodeId_ && *selectedNodeId_ == nodeVisual.id) {
            g.setColour(toColour(theme.accent));
            g.drawRoundedRectangle(nodeBounds.expanded(4.0F), kCornerRadius + 4.0F, 2.0F);
        }

        if (swapTargetId_ && *swapTargetId_ == nodeVisual.id) {
            g.setColour(toColour(theme.accent).withAlpha(0.45F));
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
            g.setColour(toColour(theme.meterPeak));
            g.fillRect(filledBounds);
            g.setColour(toColour(theme.textPrimary).withAlpha(0.3F));
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
    const auto accentColour = toColour(theme.accent).withAlpha(0.7F);

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

        auto colour = accentColour.withAlpha(0.5F);
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

    if (!draggingNodeId_ || view_ == nullptr) {
        return;
    }

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

    const auto normX = juce::jlimit(0.0F, 1.0F, (center.x - layoutArea_.getX()) / width);
    const auto normY = juce::jlimit(0.0F, 1.0F, (center.y - layoutArea_.getY()) / height);

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

    auto normX = (position.x - layoutArea_.getX()) / width;
    auto normY = (position.y - layoutArea_.getY()) / height;
    normX = juce::jlimit(0.0F, 1.0F, normX);
    normY = juce::jlimit(0.0F, 1.0F, normY);

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
    resolveFixedEndpoints();
    refreshCachedPositions();
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

void NodeGraphComponent::refreshCachedPositions() {
    layoutArea_ = computeLayoutArea();
    cachedPositions_.clear();
    if (view_ == nullptr) {
        return;
    }

    cachedPositions_.reserve(view_->nodes().size());

    for (const auto& node : view_->nodes()) {
        auto normPos = juce::Point<float>(node.normX, node.normY);
        const auto clampedNorm = juce::Point<float>(
            juce::jlimit(0.0F, 1.0F, normPos.x),
            juce::jlimit(0.0F, 1.0F, normPos.y));
        const auto centerX = layoutArea_.getX() + clampedNorm.x * layoutArea_.getWidth();
        const auto centerY = layoutArea_.getY() + clampedNorm.y * layoutArea_.getHeight();
        cachedPositions_.emplace(node.id, juce::Point<float>(centerX, centerY));
    }
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
    editor->setIndents(2, 2);
    editor->setReturnKeyStartsNewLine(false);
    editor->setJustification(juce::Justification::centred);
    editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromFloatRGBA(0.0F, 0.0F, 0.0F, 0.2F));
    editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor->setColour(juce::TextEditor::focusedOutlineColourId, toColour(view_->theme().accent));
    editor->setColour(juce::TextEditor::textColourId, toColour(view_->theme().textPrimary));
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
