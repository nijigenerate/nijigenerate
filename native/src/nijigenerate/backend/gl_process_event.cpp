#include "nijigenerate/backend/gl.hpp"
#include <imgui_impl_sdl3.h>

namespace nijigenerate::backend::gl {

bool ngGLBackendProcessEvent(const SDL_Event* event) {
    // Compatibility entry only: the single project platform adapter owns input.
    return ImGui_ImplSDL3_ProcessEvent(event);
}

} // namespace nijigenerate::backend::gl
