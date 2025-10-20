#include "core/Application.h"
#include "persistence/ProjectSerializer.h"

#include <cassert>
#include <filesystem>

int main() {
    broadcastmix::core::Application app({ .appName = "BroadcastMix", .version = "3.0.0" },
                                        {});
    try {
        app.initialize();
    } catch (...) {
        assert(false && "Application initialization threw unexpectedly");
    }

    namespace fs = std::filesystem;
    const auto testsDir = fs::current_path();
    const auto projectRoot = testsDir.parent_path().parent_path();
    const auto sampleProjectPath = projectRoot / "projects" / "SampleService.broadcastmix";

    broadcastmix::persistence::ProjectSerializer serializer;
    auto sampleProject = serializer.load(sampleProjectPath.string());
    assert(sampleProject.graphTopology && !sampleProject.graphTopology->nodes().empty());
    assert(!sampleProject.snapshotNames.empty());
    assert(sampleProject.lastAutosavePath.has_value());

    const auto tempRoot = fs::temp_directory_path() / "broadcastmix_project_serializer_test";
    fs::remove_all(tempRoot);
    serializer.save(sampleProject, tempRoot.string());
    auto reloaded = serializer.load(tempRoot.string());
    assert(reloaded.graphTopology && reloaded.graphTopology->nodes().size() == sampleProject.graphTopology->nodes().size());
    assert(reloaded.snapshotNames == sampleProject.snapshotNames);
    assert(reloaded.lastAutosavePath.has_value());
    fs::remove_all(tempRoot);

    return 0;
}
