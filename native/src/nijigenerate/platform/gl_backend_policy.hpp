#pragma once
#include <array>

namespace nijigenerate::platform {
bool ngGLBackendManuallyScalesViewport() noexcept;
std::array<char, 32> ngGLBackendInitialGLSLVersion() noexcept;
} // namespace nijigenerate::platform
