#include "MeterStore.h"

#include <algorithm>
#include <unordered_set>

namespace broadcastmix::audio {

MeterStore::MeterValue::MeterValue() {
    channels[0].store(0.0F, std::memory_order_relaxed);
    channels[1].store(0.0F, std::memory_order_relaxed);
}

MeterStore::MeterStore() = default;
MeterStore::~MeterStore() = default;

MeterStore::MeterPtr MeterStore::meterFor(const std::string& nodeId) {
    std::scoped_lock lock(mutex_);
    if (const auto it = meters_.find(nodeId); it != meters_.end()) {
        return it->second;
    }
    return createMeterLocked(nodeId);
}

std::array<float, 2> MeterStore::levelsFor(const std::string& nodeId) const {
    std::scoped_lock lock(mutex_);
    std::array<float, 2> levels { 0.0F, 0.0F };
    if (const auto it = meters_.find(nodeId); it != meters_.end() && it->second) {
        for (std::size_t channel = 0; channel < levels.size(); ++channel) {
            levels[channel] = std::clamp(it->second->channels[channel].load(std::memory_order_relaxed), 0.0F, 1.0F);
        }
    }
    return levels;
}

void MeterStore::syncWithTopology(const GraphTopology& topology) {
    std::unordered_set<std::string> ids;
    ids.reserve(topology.nodes().size());
    for (const auto& node : topology.nodes()) {
        ids.insert(node.id());
    }

    std::scoped_lock lock(mutex_);
    for (const auto& id : ids) {
        if (!meters_.contains(id)) {
            meters_.emplace(id, std::make_shared<MeterValue>());
        }
    }

    for (auto it = meters_.begin(); it != meters_.end();) {
        if (!ids.contains(it->first)) {
            it = meters_.erase(it);
        } else {
            ++it;
        }
    }
}

MeterStore::MeterPtr MeterStore::createMeterLocked(const std::string& nodeId) {
    auto meter = std::make_shared<MeterValue>();
    meters_.emplace(nodeId, meter);
    return meter;
}

} // namespace broadcastmix::audio
