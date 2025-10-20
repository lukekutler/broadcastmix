#pragma once

#include <string>
#include <vector>

namespace broadcastmix::control {

struct ControlSurface {
    std::string id;
    std::string name;
    bool isConnected { false };
};

class ControlSurfaceManager {
public:
    ControlSurfaceManager();

    void discover();
    void connect(const std::string& id);
    void disconnect(const std::string& id);

    [[nodiscard]] const std::vector<ControlSurface>& surfaces() const noexcept;

private:
    std::vector<ControlSurface> surfaces_;
};

} // namespace broadcastmix::control
