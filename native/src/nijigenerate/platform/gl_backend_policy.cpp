#include "nijigenerate/platform/gl_backend_policy.hpp"
#include "nijigenerate/core/dpi.hpp"

namespace nijigenerate::platform {

bool ngGLBackendManuallyScalesViewport() noexcept {
#if defined(NG_USE_UI_SCALING) && !defined(NG_OSX)
    return true;
#else
    return false;
#endif
}

std::array<char, 32> ngGLBackendInitialGLSLVersion() noexcept {
#if defined(NG_OSX)
    return {{'#', 'v', 'e', 'r', 's', 'i', 'o', 'n', ' ', '3', '3', '0'}};
#else
    return {{'#', 'v', 'e', 'r', 's', 'i', 'o', 'n', ' ', '1', '3', '0'}};
#endif
}

} // namespace nijigenerate::platform
