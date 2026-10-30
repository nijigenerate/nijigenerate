#pragma once

#if defined(_WIN32)
#include <nijigenerate/platform/window_win32_dpi.hpp>

namespace nijigenerate::core {
// Same source-value enum identities as the actual SDK-boundary implementation.
// Their camel-case members retain source integers, not SDK pointer constants.
using DPIAwarenessContext = ::nijigenerate::platform::detail::SourceDPIAwarenessContext;
using ProcessDPIAwareness = ::nijigenerate::platform::detail::SourceProcessDPIAwareness;

// Public Windows-only entry corresponding to core.window.incSetWin32DPIAwareness.
void ngSetWin32DPIAwareness();
}
#endif
