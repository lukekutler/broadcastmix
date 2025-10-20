#pragma once

#include "../audio/GraphTopology.h"
#include "UiTheme.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace broadcastmix::ui {

class NodeGraphView {
public:
    struct PositionOverride {
        float normX { 0.5F };
        float normY { 0.5F };
    };

    using PositionOverrideMap = std::unordered_map<std::string, PositionOverride>;

    struct NodeVisual {
        std::string id;
        std::string label;
        audio::GraphNodeType type;
        float normX;
        float normY;
        bool enabled { true };
        std::uint32_t inputChannels { 0 };
        std::uint32_t outputChannels { 0 };
    };

    struct ConnectionVisual {
        std::string fromId;
        std::string toId;
        std::uint32_t fromPort { 0 };
        std::uint32_t toPort { 0 };
    };

    NodeGraphView();

    void loadTheme(const UiTheme& theme);
    void setTopology(std::shared_ptr<const audio::GraphTopology> topology);

    void runEventLoop();

    [[nodiscard]] const UiTheme& theme() const noexcept;
    [[nodiscard]] const std::vector<NodeVisual>& nodes() const noexcept;
    [[nodiscard]] const std::vector<ConnectionVisual>& connections() const noexcept;
    [[nodiscard]] std::size_t layoutVersion() const noexcept;
    [[nodiscard]] const PositionOverrideMap& positionOverrides() const noexcept;
    void setPositionOverride(const std::string& nodeId, float normX, float normY);
    void clearPositionOverride(const std::string& nodeId);
    void setPositionOverrides(PositionOverrideMap overrides);

private:
    void rebuildLayout();

    UiTheme theme_;
    std::shared_ptr<const audio::GraphTopology> topology_;
    std::vector<NodeVisual> nodes_;
    std::vector<ConnectionVisual> connections_;
    std::size_t layoutVersion_ { 0 };
    PositionOverrideMap overrides_;
};

} // namespace broadcastmix::ui
