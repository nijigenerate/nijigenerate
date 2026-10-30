#include "nijigenerate/backend/gl.hpp"
#include <imgui_impl_sdl3.h>

namespace nijigenerate::backend::gl {

void ngGLBackendPlatformNewFrame() {
    // Fallback authority/scaling happen at platform emission, not queue surgery.
    ImGui_ImplSDL3_NewFrame();
}

} // namespace nijigenerate::backend::gl
