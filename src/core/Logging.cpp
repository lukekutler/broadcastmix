#include "Logging.h"

#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace broadcastmix::core {

namespace {

std::string categoryTag(LogCategory category) {
    switch (category) {
    case LogCategory::Audio:
        return "[audio]";
    case LogCategory::Lifecycle:
        return "[lifecycle]";
    case LogCategory::Persistence:
        return "[persistence]";
    case LogCategory::Plugin:
        return "[plugin]";
    case LogCategory::Ui:
        return "[ui]";
    case LogCategory::Control:
        return "[control]";
    case LogCategory::Update:
        return "[update]";
    default:
        return "[unknown]";
    }
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

void log(LogCategory category, std::string_view message) {
    std::cout << timestamp() << ' ' << categoryTag(category) << ' ' << message << std::endl;
}

} // namespace broadcastmix::core
