#include "nijigenerate/backend/gl.hpp"

namespace nijigenerate::backend::gl {

void ngGLBackendPlatformInterfaceShutdown() {
    ImGui::DestroyPlatformWindows();
}

} // namespace nijigenerate::backend::gl
