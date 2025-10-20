#pragma once

#include <format>
#include <string>
#include <string_view>

namespace broadcastmix::core {

enum class LogCategory {
    Audio,
    Lifecycle,
    Persistence,
    Plugin,
    Ui,
    Control,
    Update,
};

void log(LogCategory category, std::string_view message);

template <typename... Args>
void log(LogCategory category, std::format_string<Args...> fmt, Args&&... args) {
    log(category, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace broadcastmix::core
