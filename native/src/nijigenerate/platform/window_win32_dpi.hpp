#pragma once

#if defined(_WIN32)
#include <cstdint>

namespace nijigenerate::platform::detail {

// These are the editor source values, not Windows pointer pseudo-handles.
enum class SourceDPIAwarenessContext : std::int32_t {
    unaware = 0,
    systemAware = 1,
    perMonitorAware = 2,
    perMonitorAwareV2 = 3,
};

enum class SourceProcessDPIAwareness : std::int32_t {
    unaware = 0,
    systemAware = 1,
    perMonitorAware = 2,
};

// SDK-boundary implementation of the public core Windows-only entry.
void setWin32DPIAwareness();

} // namespace nijigenerate::platform::detail
#endif
