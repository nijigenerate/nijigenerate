#pragma once

#include "nijigenerate/backend/gl.hpp"
#include <cstdint>

namespace nijigenerate::backend::gl::detail {

// The integer overload preserves the primary source input domain. SDL3 event
// callers keep fractional coordinates through the separate float domain.
ImVec2 scaleMousePosition(std::int32_t x, std::int32_t y);
ImVec2 scaleEventMousePosition(float x, float y);
void rememberEventMousePosition(std::uint32_t windowID, std::int32_t x, std::int32_t y);
void rememberSDLEventMousePosition(std::uint32_t windowID, float x, float y);

} // namespace nijigenerate::backend::gl::detail
