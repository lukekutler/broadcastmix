#pragma once

#include <string>

namespace broadcastmix::update {

class UpdateService {
public:
    UpdateService();

    void initialize(const std::string& version);
    void checkForUpdates();

private:
    std::string currentVersion_;
};

} // namespace broadcastmix::update
