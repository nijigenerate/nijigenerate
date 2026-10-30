#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/platform/gl_backend_policy.hpp"
#include <cmath>

namespace nijigenerate::backend::gl {

void ngGLBackendBeginRender() {
    if (platform::ngGLBackendManuallyScalesViewport()) {
        const float scale = core::dpi::incGetUIScale();
        auto* viewport = ImGui::GetMainViewport();
        viewport->WorkSize.x = static_cast<float>(std::ceil(
            static_cast<double>(viewport->WorkSize.x) / static_cast<double>(scale)));
        viewport->WorkSize.y = static_cast<float>(std::ceil(
            static_cast<double>(viewport->WorkSize.y) / static_cast<double>(scale)));
        viewport->Size.x = static_cast<float>(std::ceil(
            static_cast<double>(viewport->Size.x) / static_cast<double>(scale)));
        viewport->Size.y = static_cast<float>(std::ceil(
            static_cast<double>(viewport->Size.y) / static_cast<double>(scale)));
        viewport->WorkSize.y -= (26 + 10) * (scale - 1);
    }
}

} // namespace nijigenerate::backend::gl
