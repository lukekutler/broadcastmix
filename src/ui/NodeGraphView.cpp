#include "NodeGraphView.h"

#include "../core/Logging.h"

#include <algorithm>
#include <array>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <string_view>

namespace broadcastmix::ui {

namespace {

float normalisedCoordinate(std::size_t index, std::size_t count) {
    if (count <= 1) {
        return 0.5F;
    }
    return static_cast<float>(index + 1U) / static_cast<float>(count + 1U);
}

} // namespace

NodeGraphView::NodeGraphView() = default;

void NodeGraphView::loadTheme(const UiTheme& theme) {
    theme_ = theme;
    core::log(core::LogCategory::Ui, "Theme loaded with font {}", theme.fontFamily);
}

void NodeGraphView::setTopology(std::shared_ptr<const audio::GraphTopology> topology) {
    topology_ = std::move(topology);
    rebuildLayout();
    core::log(core::LogCategory::Ui, "Topology assigned to node graph view");
}

void NodeGraphView::runEventLoop() {
    core::log(core::LogCategory::Ui, "Entering UI event loop (stub)");
    // TODO: Integrate with JUCE/Swift UI event loop.
}

const UiTheme& NodeGraphView::theme() const noexcept {
    return theme_;
}

const std::vector<NodeGraphView::NodeVisual>& NodeGraphView::nodes() const noexcept {
    return nodes_;
}

const std::vector<NodeGraphView::ConnectionVisual>& NodeGraphView::connections() const noexcept {
    return connections_;
}

std::size_t NodeGraphView::layoutVersion() const noexcept {
    return layoutVersion_;
}

const NodeGraphView::PositionOverrideMap& NodeGraphView::positionOverrides() const noexcept {
    return overrides_;
}

void NodeGraphView::setPositionOverride(const std::string& nodeId, float normX, float normY) {
    overrides_[nodeId] = PositionOverride {
        .normX = std::clamp(normX, 0.0F, 1.0F),
        .normY = std::clamp(normY, 0.0F, 1.0F)
    };
    rebuildLayout();
}

void NodeGraphView::clearPositionOverride(const std::string& nodeId) {
    overrides_.erase(nodeId);
    rebuildLayout();
}

void NodeGraphView::setPositionOverrides(PositionOverrideMap overrides) {
    for (auto& [id, position] : overrides) {
        position.normX = std::clamp(position.normX, 0.0F, 1.0F);
        position.normY = std::clamp(position.normY, 0.0F, 1.0F);
    }
    overrides_ = std::move(overrides);
    rebuildLayout();
}

void NodeGraphView::rebuildLayout() {
    nodes_.clear();
    connections_.clear();
    layoutVersion_++;

    if (!topology_) {
        return;
    }

    const auto& graphNodes = topology_->nodes();
    const auto& graphConnections = topology_->connections();
    if (graphNodes.empty()) {
        return;
    }

    constexpr std::array<std::pair<std::string_view, std::size_t>, 11> columnAssignments {
        std::pair { "band_group", std::size_t { 0 } },
        std::pair { "vocal_group", std::size_t { 0 } },
        std::pair { "communication_group", std::size_t { 0 } },
        std::pair { "misc_group", std::size_t { 0 } },
        std::pair { "broadcast_bus", std::size_t { 1 } },
        std::pair { "broadcast_output", std::size_t { 2 } },
        std::pair { "monitor_trim", std::size_t { 3 } },
        std::pair { "utility_channels", std::size_t { 4 } },
        std::pair { "monitor_bus", std::size_t { 5 } },
        std::pair { "monitor_output", std::size_t { 6 } }
    };

    std::unordered_map<std::string, std::size_t> columnIndices;
    std::unordered_set<std::string> fixedColumns;
    columnIndices.reserve(graphNodes.size());
    std::size_t maxColumnIndex = 0;

    for (const auto& [id, column] : columnAssignments) {
        const auto idString = std::string(id);
        columnIndices[idString] = column;
        fixedColumns.insert(idString);
        maxColumnIndex = std::max(maxColumnIndex, column);
    }

    std::unordered_map<std::string, const audio::GraphNode*> nodeLookup;
    nodeLookup.reserve(graphNodes.size());
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    adjacency.reserve(graphConnections.size());
    std::unordered_map<std::string, std::size_t> indegree;
    indegree.reserve(graphNodes.size());

    for (const auto& node : graphNodes) {
        nodeLookup.emplace(node.id(), &node);
        adjacency[node.id()]; // ensure entry exists
        indegree[node.id()] = 0;
    }

    for (const auto& connection : graphConnections) {
        adjacency[connection.fromNodeId].push_back(connection.toNodeId);
        indegree[connection.toNodeId] += 1;
    }

    std::queue<std::string> queue;
    for (const auto& [id, degree] : indegree) {
        if (degree == 0) {
            queue.push(id);
        }
    }

    while (!queue.empty()) {
        auto currentId = queue.front();
        queue.pop();

        const auto baseColumn = columnIndices.contains(currentId) ? columnIndices.at(currentId) : 0U;
        for (const auto& neighbour : adjacency[currentId]) {
            if (!fixedColumns.contains(neighbour)) {
                const auto proposedColumn = baseColumn + 1U;
                auto& neighbourColumn = columnIndices[neighbour];
                neighbourColumn = std::max(neighbourColumn, proposedColumn);
                maxColumnIndex = std::max(maxColumnIndex, neighbourColumn);
            }

            auto& neighbourIndegree = indegree[neighbour];
            if (neighbourIndegree > 0) {
                neighbourIndegree -= 1;
                if (neighbourIndegree == 0) {
                    queue.push(neighbour);
                }
            }
        }
    }

    for (const auto& node : graphNodes) {
        if (!columnIndices.contains(node.id())) {
            columnIndices.emplace(node.id(), 0);
        }
        maxColumnIndex = std::max(maxColumnIndex, columnIndices.at(node.id()));
    }

    std::vector<std::vector<const audio::GraphNode*>> columns(maxColumnIndex + 1);
    for (const auto& node : graphNodes) {
        const auto columnIdx = columnIndices.at(node.id());
        if (columnIdx >= columns.size()) {
            columns.resize(columnIdx + 1);
        }
        columns[columnIdx].push_back(&node);
    }

    const std::unordered_map<std::string_view, std::size_t> groupOrder {
        { "band_group", 0 },
        { "vocal_group", 1 },
        { "communication_group", 2 },
        { "misc_group", 3 }
    };

    for (std::size_t columnIdx = 0; columnIdx < columns.size(); ++columnIdx) {
        auto& columnNodes = columns[columnIdx];
        if (columnNodes.empty()) {
            continue;
        }

        if (columnIdx == 0) {
            std::sort(columnNodes.begin(), columnNodes.end(), [&](const audio::GraphNode* lhs, const audio::GraphNode* rhs) {
                const auto lhsOrder = groupOrder.contains(lhs->id()) ? groupOrder.at(lhs->id()) : static_cast<std::size_t>(groupOrder.size());
                const auto rhsOrder = groupOrder.contains(rhs->id()) ? groupOrder.at(rhs->id()) : static_cast<std::size_t>(groupOrder.size());
                if (lhsOrder == rhsOrder) {
                    return lhs->id() < rhs->id();
                }
                return lhsOrder < rhsOrder;
            });
        } else {
            std::sort(columnNodes.begin(), columnNodes.end(), [](const audio::GraphNode* lhs, const audio::GraphNode* rhs) {
                const auto& lhsLabel = lhs->label().empty() ? lhs->id() : lhs->label();
                const auto& rhsLabel = rhs->label().empty() ? rhs->id() : rhs->label();
                return lhsLabel < rhsLabel;
            });
        }
    }

    std::unordered_map<std::string, float> xPositions;
    xPositions.reserve(graphNodes.size());

    const auto columnCount = std::max<std::size_t>(columns.size(), 2);
    for (std::size_t columnIdx = 0; columnIdx < columns.size(); ++columnIdx) {
        const auto denominator = static_cast<float>(std::max<std::size_t>(1, columnCount - 1));
        const auto x = static_cast<float>(columnIdx) / denominator;
        auto& columnNodes = columns[columnIdx];
        for (std::size_t rowIndex = 0; rowIndex < columnNodes.size(); ++rowIndex) {
            const auto* node = columnNodes[rowIndex];
            auto displayLabel = node->label().empty() ? node->id() : node->label();
            const auto person = node->person();
            const auto role = node->role();
            const auto source = node->source();
            const auto profileImage = node->profileImagePath();
            const auto preset = node->presetName();
            if (node->type() == audio::GraphNodeType::Position && !person.empty()) {
                displayLabel = person;
            }

            NodeVisual visual {
                .id = node->id(),
                .label = displayLabel,
                .type = node->type(),
                .normX = x,
                .normY = normalisedCoordinate(rowIndex, columnNodes.size()),
                .enabled = node->enabled(),
                .inputChannels = std::min<std::uint32_t>(node->inputChannelCount(), 2U),
                .outputChannels = std::min<std::uint32_t>(node->outputChannelCount(), 2U),
                .person = person,
                .role = role,
                .source = source,
                .profileImagePath = profileImage,
                .preset = preset
            };
            xPositions.emplace(visual.id, x);
            nodes_.push_back(std::move(visual));
        }
    }

    for (auto& visual : nodes_) {
        if (const auto it = overrides_.find(visual.id); it != overrides_.end()) {
            visual.normX = it->second.normX;
            visual.normY = it->second.normY;
            xPositions[visual.id] = visual.normX;
        }
    }

    std::unordered_set<std::string> seenPairs;
    seenPairs.reserve(graphConnections.size());

    connections_.reserve(graphConnections.size());
    for (const auto& connection : graphConnections) {
        if (xPositions.find(connection.fromNodeId) == xPositions.end() ||
            xPositions.find(connection.toNodeId) == xPositions.end()) {
            continue;
        }

        const auto key = connection.fromNodeId + "->" + connection.toNodeId;
        if (!seenPairs.insert(key).second) {
            continue;
        }

        connections_.push_back(ConnectionVisual {
            .fromId = connection.fromNodeId,
            .toId = connection.toNodeId,
            .fromPort = 0,
            .toPort = 0
        });
    }
}

} // namespace broadcastmix::ui
