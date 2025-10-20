#pragma once

#include <cstdint>
#include <string>

namespace broadcastmix::ui {

struct Color {
    float r;
    float g;
    float b;
    float a;
};

struct UiTheme {
    Color background;
    Color accent;
    Color textPrimary;
    Color meterPeak;
    std::string fontFamily;

    static UiTheme createDefault();
};

} // namespace broadcastmix::ui
