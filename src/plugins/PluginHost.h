#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace broadcastmix::plugins {

struct PluginDescriptor {
    std::string identifier;
    std::string name;
    std::string vendor;
};

class PluginInstance {
public:
    explicit PluginInstance(PluginDescriptor descriptor);
    void load();
    void unload();
    [[nodiscard]] const PluginDescriptor& descriptor() const;

private:
    PluginDescriptor descriptor_;
    bool loaded_ { false };
};

class PluginHost {
public:
    PluginHost();

    void bootstrap();
    void registerAvailablePlugin(PluginDescriptor descriptor);
    std::shared_ptr<PluginInstance> createInstance(const std::string& identifier);

    [[nodiscard]] const std::vector<PluginDescriptor>& availablePlugins() const noexcept;

private:
    std::vector<PluginDescriptor> available_;
    std::unordered_map<std::string, std::weak_ptr<PluginInstance>> activeInstances_;
};

} // namespace broadcastmix::plugins
