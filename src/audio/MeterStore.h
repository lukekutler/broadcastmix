#pragma once

#include "GraphTopology.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <array>

namespace broadcastmix::audio {

class MeterStore {
public:
    MeterStore();
    ~MeterStore();

    struct MeterValue {
        MeterValue();
        std::array<std::atomic<float>, 2> channels;
    };

    using MeterPtr = std::shared_ptr<MeterValue>;

    MeterPtr meterFor(const std::string& nodeId);
    std::array<float, 2> levelsFor(const std::string& nodeId) const;
    void syncWithTopology(const GraphTopology& topology);

private:
    MeterPtr createMeterLocked(const std::string& nodeId);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, MeterPtr> meters_;
};

} // namespace broadcastmix::audio
