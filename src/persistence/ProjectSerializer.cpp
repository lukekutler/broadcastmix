#include "ProjectSerializer.h"

#include "../core/Logging.h"

#include <array>
#include <fstream>
#include <sstream>
#include <unordered_map>

#if BROADCASTMIX_HAS_JUCE
#include <juce_data_structures/juce_data_structures.h>
#endif

namespace broadcastmix::persistence {

namespace {

namespace fs = std::filesystem;

constexpr std::array<const char*, 5> kProjectSubdirectories {
    "snapshots",
    "media",
    "captures",
    "autosave",
    "logs"
};

constexpr const char* kGraphFileName = "graph.json";
constexpr const char* kSnapshotIndexFileName = "index.json";
constexpr const char* kAutosaveGraphFileName = "graph.json";

std::string nodeTypeToString(audio::GraphNodeType type) {
    switch (type) {
    case audio::GraphNodeType::Input:
        return "Input";
    case audio::GraphNodeType::Channel:
        return "Channel";
    case audio::GraphNodeType::GroupBus:
        return "GroupBus";
    case audio::GraphNodeType::Person:
        return "Person";
    case audio::GraphNodeType::BroadcastBus:
        return "BroadcastBus";
    case audio::GraphNodeType::MixBus:
        return "MixBus";
    case audio::GraphNodeType::Utility:
        return "Utility";
    case audio::GraphNodeType::Plugin:
        return "Plugin";
    case audio::GraphNodeType::SignalGenerator:
        return "SignalGenerator";
    case audio::GraphNodeType::Output:
        return "Output";
    default:
        return "Unknown";
    }
}

std::optional<audio::GraphNodeType> nodeTypeFromString(const std::string& type) {
    static const std::unordered_map<std::string, audio::GraphNodeType> lookup {
        { "Input", audio::GraphNodeType::Input },
        { "Channel", audio::GraphNodeType::Channel },
        { "GroupBus", audio::GraphNodeType::GroupBus },
        { "Person", audio::GraphNodeType::Person },
        { "Position", audio::GraphNodeType::Person },
        { "BroadcastBus", audio::GraphNodeType::BroadcastBus },
        { "MixBus", audio::GraphNodeType::MixBus },
        { "Utility", audio::GraphNodeType::Utility },
        { "Plugin", audio::GraphNodeType::Plugin },
        { "SignalGenerator", audio::GraphNodeType::SignalGenerator },
        { "Output", audio::GraphNodeType::Output },
    };

    if (const auto it = lookup.find(type); it != lookup.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ensureProjectSkeleton(const fs::path& root) {
    std::error_code ec;
    fs::create_directories(root, ec);
    for (const auto* subdir : kProjectSubdirectories) {
        fs::create_directories(root / subdir, ec);
    }
}

#if BROADCASTMIX_HAS_JUCE

juce::var topologyToVar(const audio::GraphTopology& topology) {
    auto* graphObject = new juce::DynamicObject();

    juce::Array<juce::var> nodes;
    for (const auto& node : topology.nodes()) {
        auto* nodeObj = new juce::DynamicObject();
        nodeObj->setProperty("id", juce::String(node.id()));
        nodeObj->setProperty("type", juce::String(nodeTypeToString(node.type())));
        nodeObj->setProperty("label", juce::String(node.label()));
        nodeObj->setProperty("inputs", static_cast<int>(node.inputChannelCount()));
        nodeObj->setProperty("outputs", static_cast<int>(node.outputChannelCount()));
        nodeObj->setProperty("enabled", node.enabled());
        nodeObj->setProperty("person", juce::String(node.person()));
        nodeObj->setProperty("role", juce::String(node.role()));
        nodeObj->setProperty("source", juce::String(node.source()));
        nodeObj->setProperty("profileImage", juce::String(node.profileImagePath()));
        nodeObj->setProperty("preset", juce::String(node.presetName()));
        nodes.add(juce::var(nodeObj));
    }

    juce::Array<juce::var> connections;
    for (const auto& connection : topology.connections()) {
        auto* connObj = new juce::DynamicObject();
        connObj->setProperty("from", juce::String(connection.fromNodeId));
        connObj->setProperty("fromChannel", static_cast<int>(connection.fromChannel));
        connObj->setProperty("to", juce::String(connection.toNodeId));
        connObj->setProperty("toChannel", static_cast<int>(connection.toChannel));
        connections.add(juce::var(connObj));
    }

    graphObject->setProperty("nodes", nodes);
    graphObject->setProperty("connections", connections);
    return juce::var(graphObject);
}

std::shared_ptr<audio::GraphTopology> topologyFromVar(const juce::var& varGraph) {
    if (!varGraph.isObject()) {
        return nullptr;
    }

    auto topology = std::make_shared<audio::GraphTopology>();
    const auto nodes = varGraph["nodes"];
    const auto connections = varGraph["connections"];

    if (nodes.isArray()) {
        for (const auto& nodeVar : *nodes.getArray()) {
            if (!nodeVar.isObject()) {
                continue;
            }

            const auto id = nodeVar["id"].toString().toStdString();
            const auto typeString = nodeVar["type"].toString().toStdString();
            const auto type = nodeTypeFromString(typeString);
            if (!type) {
                continue;
            }

            audio::GraphNode node(id, *type);
            node.setLabel(nodeVar["label"].toString().toStdString());

            const auto inputs = static_cast<std::uint32_t>(static_cast<int>(nodeVar["inputs"]));
            const auto outputs = static_cast<std::uint32_t>(static_cast<int>(nodeVar["outputs"]));

            for (std::uint32_t i = 0; i < inputs; ++i) {
                node.addInputChannel();
            }
            for (std::uint32_t i = 0; i < outputs; ++i) {
                node.addOutputChannel();
            }

            if (*type == audio::GraphNodeType::SignalGenerator) {
                if (node.inputChannelCount() == 0) {
                    node.addInputChannel();
                    node.addInputChannel();
                }
                if (node.outputChannelCount() == 0) {
                    node.addOutputChannel();
                    node.addOutputChannel();
                }
            }

            const auto enabledVar = nodeVar.getProperty("enabled", juce::var(true));
            node.setEnabled(static_cast<bool>(enabledVar));

            if (const auto personVar = nodeVar.getProperty("person", juce::var()); personVar.isString()) {
                node.setPerson(personVar.toString().toStdString());
            }
            if (const auto roleVar = nodeVar.getProperty("role", juce::var()); roleVar.isString()) {
                node.setRole(roleVar.toString().toStdString());
            }
            if (const auto sourceVar = nodeVar.getProperty("source", juce::var()); sourceVar.isString()) {
                node.setSource(sourceVar.toString().toStdString());
            }
            if (const auto imageVar = nodeVar.getProperty("profileImage", juce::var()); imageVar.isString()) {
                node.setProfileImagePath(imageVar.toString().toStdString());
            }
            if (const auto presetVar = nodeVar.getProperty("preset", juce::var()); presetVar.isString()) {
                node.setPresetName(presetVar.toString().toStdString());
            }

            topology->addNode(std::move(node));
        }
    }

    if (connections.isArray()) {
        for (const auto& connVar : *connections.getArray()) {
            if (!connVar.isObject()) {
                continue;
            }

            audio::GraphConnection connection {
                .fromNodeId = connVar["from"].toString().toStdString(),
                .fromChannel = static_cast<std::uint32_t>(static_cast<int>(connVar["fromChannel"])),
                .toNodeId = connVar["to"].toString().toStdString(),
                .toChannel = static_cast<std::uint32_t>(static_cast<int>(connVar["toChannel"]))
            };

            topology->connect(std::move(connection));
        }
    }

    return topology;
}

juce::var layoutMapToVar(const std::unordered_map<std::string, LayoutPosition>& layout) {
    auto* layoutObject = new juce::DynamicObject();
    for (const auto& [id, position] : layout) {
        auto* positionObject = new juce::DynamicObject();
        positionObject->setProperty("x", position.normX);
        positionObject->setProperty("y", position.normY);
        layoutObject->setProperty(juce::Identifier(id), juce::var(positionObject));
    }
    return juce::var(layoutObject);
}

void layoutMapFromVar(const juce::var& varLayout,
                      std::unordered_map<std::string, LayoutPosition>& out) {
    out.clear();
    if (!varLayout.isObject()) {
        return;
    }

    const auto* object = varLayout.getDynamicObject();
    for (const auto& property : object->getProperties()) {
        const auto value = property.value;
        if (!value.isObject()) {
            continue;
        }

        const auto x = static_cast<float>(value.getProperty("x", 0.5));
        const auto y = static_cast<float>(value.getProperty("y", 0.5));
        out.emplace(property.name.toString().toStdString(), LayoutPosition { x, y });
    }
}

juce::var microViewsToVar(const std::unordered_map<std::string, MicroViewState>& microViews) {
    auto* microObject = new juce::DynamicObject();
    for (const auto& [id, state] : microViews) {
        auto* viewObject = new juce::DynamicObject();
        if (state.topology) {
            viewObject->setProperty("graph", topologyToVar(*state.topology));
        }
        viewObject->setProperty("layout", layoutMapToVar(state.layout));
        microObject->setProperty(juce::Identifier(id), juce::var(viewObject));
    }
    return juce::var(microObject);
}

void microViewsFromVar(const juce::var& varMicro,
                       std::unordered_map<std::string, MicroViewState>& out) {
    out.clear();
    if (!varMicro.isObject()) {
        return;
    }

    const auto* object = varMicro.getDynamicObject();
    for (const auto& property : object->getProperties()) {
        const auto value = property.value;
        if (!value.isObject()) {
            continue;
        }

        MicroViewState state;
        const auto graphVar = value.getProperty("graph", juce::var());
        if (graphVar.isObject()) {
            state.topology = topologyFromVar(graphVar);
        }
        const auto layoutVar = value.getProperty("layout", juce::var());
        layoutMapFromVar(layoutVar, state.layout);
        out.emplace(property.name.toString().toStdString(), std::move(state));
    }
}

juce::var personPresetsToVar(const std::vector<PersonPresetState>& presets) {
    juce::Array<juce::var> array;
    for (const auto& preset : presets) {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", juce::String(preset.name));
        obj->setProperty("person", juce::String(preset.person));
        obj->setProperty("role", juce::String(preset.role));
        obj->setProperty("profileImage", juce::String(preset.profileImagePath));
        if (preset.topology) {
            obj->setProperty("graph", topologyToVar(*preset.topology));
        }
        obj->setProperty("layout", layoutMapToVar(preset.layout));
        array.add(juce::var(obj));
    }
    return juce::var(array);
}

void personPresetsFromVar(const juce::var& varPresets,
                            std::vector<PersonPresetState>& out) {
    out.clear();
    if (!varPresets.isArray()) {
        return;
    }

    for (const auto& entry : *varPresets.getArray()) {
        if (!entry.isObject()) {
            continue;
        }
        PersonPresetState preset;
        preset.name = entry["name"].toString().toStdString();
        preset.person = entry["person"].toString().toStdString();
        preset.role = entry["role"].toString().toStdString();
        preset.profileImagePath = entry["profileImage"].toString().toStdString();
        const auto graphVar = entry["graph"];
        if (graphVar.isObject()) {
            preset.topology = topologyFromVar(graphVar);
        }
        const auto layoutVar = entry["layout"];
        layoutMapFromVar(layoutVar, preset.layout);
        out.push_back(std::move(preset));
    }
}

std::shared_ptr<audio::GraphTopology> loadGraphFromFile(const fs::path& graphPath,
                                                       std::unordered_map<std::string, LayoutPosition>* macroLayout,
                                                       std::unordered_map<std::string, MicroViewState>* microViews,
                                                       std::vector<PersonPresetState>* personPresets = nullptr) {
    juce::File graphFile(graphPath.string());
    auto inputStream = std::unique_ptr<juce::FileInputStream>(graphFile.createInputStream());

    if (inputStream == nullptr) {
        return nullptr;
    }

    const auto jsonText = inputStream->readEntireStreamAsString();
    const auto parsed = juce::JSON::parse(jsonText);
    if (!parsed.isObject()) {
        return nullptr;
    }

    const auto graphVar = parsed["graph"];
    if (!graphVar.isObject()) {
        return nullptr;
    }

    const auto topology = topologyFromVar(graphVar);

    if ((macroLayout != nullptr || microViews != nullptr) && parsed.hasProperty("layout")) {
        const auto layoutVar = parsed["layout"];
        if (layoutVar.isObject()) {
            if (macroLayout != nullptr) {
                const auto macroVar = layoutVar["macro"];
                layoutMapFromVar(macroVar, *macroLayout);
            }
            if (microViews != nullptr) {
                const auto microVar = layoutVar["micro"];
                microViewsFromVar(microVar, *microViews);
            }
        }
    } else {
        if (macroLayout != nullptr) {
            macroLayout->clear();
        }
        if (microViews != nullptr) {
            microViews->clear();
        }
    }

    if (personPresets != nullptr) {
        if (parsed.hasProperty("personPresets")) {
            personPresetsFromVar(parsed["personPresets"], *personPresets);
        } else if (parsed.hasProperty("positionPresets")) {
            personPresetsFromVar(parsed["positionPresets"], *personPresets);
        } else {
            personPresets->clear();
        }
    }

    return topology;
}

void writeGraphToFile(const Project& project,
                      const fs::path& graphPath) {
    auto* root = new juce::DynamicObject();
    root->setProperty("name", juce::String(project.name));
    if (project.graphTopology) {
        root->setProperty("graph", topologyToVar(*project.graphTopology));
    }

    if (!project.macroLayout.empty() || !project.microViews.empty()) {
        auto* layoutObject = new juce::DynamicObject();
        if (!project.macroLayout.empty()) {
            layoutObject->setProperty("macro", layoutMapToVar(project.macroLayout));
        }
        if (!project.microViews.empty()) {
            layoutObject->setProperty("micro", microViewsToVar(project.microViews));
        }
        root->setProperty("layout", juce::var(layoutObject));
    }

    if (!project.personPresets.empty()) {
        root->setProperty("personPresets", personPresetsToVar(project.personPresets));
    }

    const auto jsonText = juce::JSON::toString(juce::var(root), true);

    juce::File graphFile(graphPath.string());
    graphFile.deleteFile();
    graphFile.create();
    graphFile.replaceWithText(jsonText);
}
#else
std::shared_ptr<audio::GraphTopology> loadGraphFromFile(const fs::path&,
                                                       std::unordered_map<std::string, LayoutPosition>* macroLayout,
                                                       std::unordered_map<std::string, MicroViewState>* microViews,
                                                       std::vector<PersonPresetState>* personPresets) {
    if (macroLayout != nullptr) {
        macroLayout->clear();
    }
    if (microViews != nullptr) {
        microViews->clear();
    }
    if (personPresets != nullptr) {
        personPresets->clear();
    }
    return nullptr;
}

void writeGraphToFile(const Project& project,
                      const fs::path& graphPath) {
    std::ofstream out(graphPath);
    out << "{\n"
        << "  \"name\": \"" << project.name << "\",\n"
        << "  \"graph\": {\n"
        << "    \"nodes\": [\n";

    const auto& nodes = project.graphTopology ? project.graphTopology->nodes() : audio::GraphNodeList {};
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        out << "      {\"id\": \"" << node.id()
            << "\", \"type\": \"" << nodeTypeToString(node.type())
            << "\", \"label\": \"" << node.label()
            << "\", \"inputs\": " << node.inputChannelCount()
            << ", \"outputs\": " << node.outputChannelCount()
            << ", \"enabled\": " << (node.enabled() ? "true" : "false") << "}";
        out << (i + 1 == nodes.size() ? "\n" : ",\n");
    }
    out << "    ],\n";

    const auto& connections = project.graphTopology ? project.graphTopology->connections() : audio::GraphConnectionList {};
    out << "    \"connections\": [\n";
    for (std::size_t i = 0; i < connections.size(); ++i) {
        const auto& connection = connections[i];
        out << "      {\"from\": \"" << connection.fromNodeId
            << "\", \"fromChannel\": " << connection.fromChannel
            << ", \"to\": \"" << connection.toNodeId
            << "\", \"toChannel\": " << connection.toChannel << "}";
        out << (i + 1 == connections.size() ? "\n" : ",\n");
    }
    out << "    ]\n";
    out << "  }\n";

    if (!project.macroLayout.empty() || !project.microViews.empty()) {
        out << "  ,\n  \"layout\": {\n";
        if (!project.macroLayout.empty()) {
            out << "    \"macro\": {\n";
            std::size_t count = 0;
            for (const auto& [id, pos] : project.macroLayout) {
                out << "      \"" << id << "\": {\"x\": " << pos.normX << ", \"y\": " << pos.normY << "}";
                out << (++count == project.macroLayout.size() ? "\n" : ",\n");
            }
            out << "    }" << (project.microViews.empty() ? "\n" : ",\n");
        }

        if (!project.microViews.empty()) {
            out << "    \"micro\": {\n";
            std::size_t mvCount = 0;
            for (const auto& [id, state] : project.microViews) {
                out << "      \"" << id << "\": {\n";
                out << "        \"layout\": {\n";
                std::size_t layoutCount = 0;
                for (const auto& [nodeId, pos] : state.layout) {
                    out << "          \"" << nodeId << "\": {\"x\": " << pos.normX << ", \"y\": " << pos.normY << "}";
                    out << (++layoutCount == state.layout.size() ? "\n" : ",\n");
                }
                out << "        }\n";
                out << "      }" << (++mvCount == project.microViews.size() ? "\n" : ",\n");
            }
            out << "    }\n";
        }
        out << "  }\n";
    }

    if (!project.personPresets.empty()) {
        out << "  ,\n  \"personPresets\": [\n";
        for (std::size_t i = 0; i < project.personPresets.size(); ++i) {
            const auto& preset = project.personPresets[i];
            out << "    {\"name\": \"" << preset.name
                << "\", \"person\": \"" << preset.person
                << "\", \"role\": \"" << preset.role
                << "\", \"profileImage\": \"" << preset.profileImagePath << "\"}";
            out << (i + 1 == project.personPresets.size() ? "\n" : ",\n");
        }
        out << "  ]\n";
    }

    out << "}\n";
}
#endif

#if BROADCASTMIX_HAS_JUCE
juce::var readJsonFile(const fs::path& path) {
    juce::File file(path.string());
    if (!file.existsAsFile()) {
        return {};
    }
    auto input = std::unique_ptr<juce::FileInputStream>(file.createInputStream());
    if (input == nullptr) {
        return {};
    }
    return juce::JSON::parse(input->readEntireStreamAsString());
}

void writeJsonToFile(const juce::var& data, const fs::path& path) {
    juce::File file(path.string());
    file.deleteFile();
    file.create();
    file.replaceWithText(juce::JSON::toString(data, true));
}

std::vector<std::string> loadSnapshotIndex(const fs::path& snapshotsDir) {
    const auto indexPath = snapshotsDir / kSnapshotIndexFileName;
    const auto parsed = readJsonFile(indexPath);
    std::vector<std::string> names;
    if (parsed.isObject()) {
        const auto list = parsed["snapshots"];
        if (list.isArray()) {
            for (const auto& item : *list.getArray()) {
                names.push_back(item.toString().toStdString());
            }
        }
    }
    return names;
}

void writeSnapshotIndex(const std::vector<std::string>& names, const fs::path& snapshotsDir) {
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> list;
    for (const auto& name : names) {
        list.add(juce::String(name));
    }
    root->setProperty("snapshots", list);
    writeJsonToFile(juce::var(root), snapshotsDir / kSnapshotIndexFileName);
}
#else
std::vector<std::string> loadSnapshotIndex(const fs::path& snapshotsDir) {
    const auto indexPath = snapshotsDir / kSnapshotIndexFileName;
    std::ifstream in(indexPath);
    std::vector<std::string> names;
    if (!in.is_open()) {
        return names;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            names.push_back(line);
        }
    }
    return names;
}

void writeSnapshotIndex(const std::vector<std::string>& names, const fs::path& snapshotsDir) {
    const auto indexPath = snapshotsDir / kSnapshotIndexFileName;
    std::ofstream out(indexPath, std::ios::trunc);
    for (const auto& name : names) {
        out << name << "\n";
    }
}
#endif

std::vector<std::string> ensureSnapshots(const fs::path& snapshotsDir) {
    if (!fs::exists(snapshotsDir / kSnapshotIndexFileName)) {
        const std::vector<std::string> defaults { "Service Default" };
        writeSnapshotIndex(defaults, snapshotsDir);
        return defaults;
    }
    auto names = loadSnapshotIndex(snapshotsDir);
    if (names.empty()) {
        names = { "Service Default" };
        writeSnapshotIndex(names, snapshotsDir);
    }
    return names;
}

std::optional<std::string> locateAutosaveGraph(const fs::path& autosaveDir) {
    const auto autosaveGraph = autosaveDir / kAutosaveGraphFileName;
    if (fs::exists(autosaveGraph)) {
        return autosaveGraph.string();
    }
    return std::nullopt;
}

} // namespace

ProjectSerializer::ProjectSerializer() = default;

Project ProjectSerializer::load(const std::string& path) {
    core::log(core::LogCategory::Persistence, "Loading project from {}", path);

    const fs::path projectPath { path };
    ensureProjectSkeleton(projectPath);

    Project project {};
    project.name = projectPath.filename().string();

    const auto graphPath = projectPath / kGraphFileName;
    std::shared_ptr<audio::GraphTopology> topology;

    if (fs::exists(graphPath)) {
        topology = loadGraphFromFile(graphPath, &project.macroLayout, &project.microViews, &project.personPresets);
    }

    if (!topology) {
        topology = std::make_shared<audio::GraphTopology>(audio::GraphTopology::createDefaultBroadcastLayout());
        project.graphTopology = topology;
        writeGraphToFile(project, graphPath);
    } else {
        project.graphTopology = std::move(topology);
    }

    const auto snapshotsDir = projectPath / "snapshots";
    project.snapshotNames = ensureSnapshots(snapshotsDir);
    project.lastAutosavePath = locateAutosaveGraph(projectPath / "autosave");
    return project;
}

void ProjectSerializer::save(const Project& project, const std::string& path) {
    core::log(core::LogCategory::Persistence, "Saving project {} to {}", project.name, path);

    const fs::path projectPath { path };
    ensureProjectSkeleton(projectPath);

    Project writableProject = project;
    if (writableProject.name.empty()) {
        writableProject.name = projectPath.filename().string();
    }

    writeGraphToFile(writableProject, projectPath / kGraphFileName);

    const auto snapshotsDir = projectPath / "snapshots";
    if (!project.snapshotNames.empty()) {
        writeSnapshotIndex(project.snapshotNames, snapshotsDir);
    } else if (!fs::exists(snapshotsDir / kSnapshotIndexFileName)) {
        writeSnapshotIndex({ "Service Default" }, snapshotsDir);
    }

    if (project.lastAutosavePath) {
        const fs::path autosaveGraph = projectPath / "autosave" / kAutosaveGraphFileName;
        if (project.graphTopology) {
            writeGraphToFile(writableProject, autosaveGraph);
        } else if (!fs::exists(autosaveGraph)) {
            fs::copy_file(project.lastAutosavePath.value(), autosaveGraph,
                          fs::copy_options::overwrite_existing);
        }
    }
}

std::filesystem::path ProjectSerializer::autosavePath(const std::filesystem::path& projectPath) const {
    return projectPath / "autosave";
}

} // namespace broadcastmix::persistence
