#include "ControlSurfaceManager.h"

#include "../core/Logging.h"

#include <algorithm>

namespace broadcastmix::control {

ControlSurfaceManager::ControlSurfaceManager() = default;

void ControlSurfaceManager::discover() {
    core::log(core::LogCategory::Control, "Discovering control surfaces");
    surfaces_.push_back({ "streamdeck", "Stream Deck", false });
}

void ControlSurfaceManager::connect(const std::string& id) {
    const auto it = std::find_if(surfaces_.begin(), surfaces_.end(),
                                 [&](const ControlSurface& surface) { return surface.id == id; });
    if (it == surfaces_.end()) {
        core::log(core::LogCategory::Control, "Surface {} not found", id);
        return;
    }
    it->isConnected = true;
    core::log(core::LogCategory::Control, "Connected control surface {}", id);
}

void ControlSurfaceManager::disconnect(const std::string& id) {
    const auto it = std::find_if(surfaces_.begin(), surfaces_.end(),
                                 [&](const ControlSurface& surface) { return surface.id == id; });
    if (it == surfaces_.end()) {
        return;
    }
    it->isConnected = false;
    core::log(core::LogCategory::Control, "Disconnected control surface {}", id);
}

const std::vector<ControlSurface>& ControlSurfaceManager::surfaces() const noexcept {
    return surfaces_;
}

} // namespace broadcastmix::control
