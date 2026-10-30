#pragma once
#include "nijigenerate/core/window_platform.hpp"

namespace nijigenerate::core::platform::detail {
struct WindowPlatformState {
    bool isWayland = false;
};
WindowPlatformState& windowPlatformState();
}
