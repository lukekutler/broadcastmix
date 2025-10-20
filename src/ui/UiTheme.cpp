#include "UiTheme.h"

namespace broadcastmix::ui {

UiTheme UiTheme::createDefault() {
    return UiTheme {
        .background = { 0.07F, 0.07F, 0.09F, 1.0F },
        .accent = { 0.33F, 0.46F, 0.66F, 1.0F },
        .textPrimary = { 0.84F, 0.84F, 0.86F, 1.0F },
        .meterPeak = { 0.84F, 0.31F, 0.23F, 1.0F },
        .fontFamily = "Inter"
    };
}

} // namespace broadcastmix::ui
