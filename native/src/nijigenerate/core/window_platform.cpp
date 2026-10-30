#include "nijigenerate/core/window_platform_internal.hpp"

namespace nijigenerate::core::platform::detail {
WindowPlatformState& windowPlatformState() {
    static thread_local WindowPlatformState state;
    return state;
}
}

namespace nijigenerate::core {
bool ngIsWayland() {
    return platform::detail::windowPlatformState().isWayland;
}
}
