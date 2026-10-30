#pragma once

#include <string_view>

namespace nijigenerate::ver {

// Preserve the current independent source literal without invoking its generator.
inline constexpr std::string_view INC_VERSION = "v1.0.0-beta2-81-g1e6496f1";

} // namespace nijigenerate::ver
