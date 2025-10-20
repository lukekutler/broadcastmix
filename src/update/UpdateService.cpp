#include "UpdateService.h"

#include "../core/Logging.h"

namespace broadcastmix::update {

UpdateService::UpdateService() = default;

void UpdateService::initialize(const std::string& version) {
    currentVersion_ = version;
    core::log(core::LogCategory::Update, "Update service initialized at version {}", version);
}

void UpdateService::checkForUpdates() {
    core::log(core::LogCategory::Update, "Checking for updates (Sparkle integration pending)");
}

} // namespace broadcastmix::update
