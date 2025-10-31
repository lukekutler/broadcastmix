#pragma once

#include "../audio/GraphTopology.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace broadcastmix::persistence {

struct LayoutPosition {
    float normX { 0.5F };
    float normY { 0.5F };
};

struct MicroViewState {
    std::shared_ptr<audio::GraphTopology> topology;
    std::unordered_map<std::string, LayoutPosition> layout;
};

struct PersonPresetState {
    std::string name;
    std::string person;
    std::string role;
    std::string profileImagePath;
    std::shared_ptr<audio::GraphTopology> topology;
    std::unordered_map<std::string, LayoutPosition> layout;
};

struct Project {
    std::string name;
    std::shared_ptr<audio::GraphTopology> graphTopology;
    std::vector<std::string> snapshotNames;
    std::optional<std::string> lastAutosavePath;
    std::unordered_map<std::string, LayoutPosition> macroLayout;
    std::unordered_map<std::string, MicroViewState> microViews;
    std::vector<PersonPresetState> personPresets;
};

class ProjectSerializer {
public:
    ProjectSerializer();

    [[nodiscard]] Project load(const std::string& path);
    void save(const Project& project, const std::string& path);

private:
    std::filesystem::path autosavePath(const std::filesystem::path& projectPath) const;
};

} // namespace broadcastmix::persistence
