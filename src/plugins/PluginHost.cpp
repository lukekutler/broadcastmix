#include "PluginHost.h"

#include "../core/Logging.h"

#include <algorithm>

namespace broadcastmix::plugins {

PluginInstance::PluginInstance(PluginDescriptor descriptor)
    : descriptor_(std::move(descriptor)) {}

void PluginInstance::load() {
    if (loaded_) {
        return;
    }
    loaded_ = true;
    core::log(core::LogCategory::Plugin, "Loaded plugin {}", descriptor_.identifier);
}

void PluginInstance::unload() {
    if (!loaded_) {
        return;
    }
    loaded_ = false;
    core::log(core::LogCategory::Plugin, "Unloaded plugin {}", descriptor_.identifier);
}

const PluginDescriptor& PluginInstance::descriptor() const {
    return descriptor_;
}

PluginHost::PluginHost() = default;

void PluginHost::bootstrap() {
    core::log(core::LogCategory::Plugin, "Bootstrapping plugin host");
    // TODO: Discover AU/VST3 plugins by scanning file system and registering descriptors.
}

void PluginHost::registerAvailablePlugin(PluginDescriptor descriptor) {
    available_.push_back(std::move(descriptor));
}

std::shared_ptr<PluginInstance> PluginHost::createInstance(const std::string& identifier) {
    const auto it = std::find_if(available_.begin(), available_.end(),
                                 [&](const auto& descriptor) { return descriptor.identifier == identifier; });
    if (it == available_.end()) {
        core::log(core::LogCategory::Plugin, "Plugin {} not available", identifier);
        return nullptr;
    }

    if (const auto cached = activeInstances_.find(identifier); cached != activeInstances_.end()) {
        if (auto locked = cached->second.lock()) {
            return locked;
        }
    }

    auto instance = std::make_shared<PluginInstance>(*it);
    instance->load();
    activeInstances_[identifier] = instance;
    return instance;
}

const std::vector<PluginDescriptor>& PluginHost::availablePlugins() const noexcept {
    return available_;
}

} // namespace broadcastmix::plugins
