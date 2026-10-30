#include "nijigenerate/backend/gl_input_internal.hpp"

namespace nijigenerate::backend::gl::detail {

ImVec2 scaleEventMousePosition(float x, float y) {
    // The real DPI owner returns one in OSX/disabled modes and its stored scale
    // in enabled modes. No second platform condition or scale cache is needed.
    const float scale = core::dpi::incGetUIScale();
    return ImVec2(x / scale, y / scale);
}

ImVec2 scaleMousePosition(std::int32_t x, std::int32_t y) {
    return scaleEventMousePosition(static_cast<float>(x), static_cast<float>(y));
}

} // namespace nijigenerate::backend::gl::detail
