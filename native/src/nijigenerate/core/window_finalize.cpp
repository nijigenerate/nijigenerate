#include "nijigenerate/core/window.hpp"
#include "nijigenerate/core/window_state_internal.hpp"
#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/backend/runtime_gl_backend.hpp"
#include "nijigenerate/commands/automesh/dynamic.hpp"
#include "nijigenerate/api/mcp/server.hpp"

#include <backends/imgui_impl_sdl3.h>

namespace nijigenerate::core {

void ngFinalize() {
    api::mcp::server::ngMcpShutdown();
    commands::automesh::dynamic::ngShutdownAutoMeshJobs();
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    backend::gl::ngGLBackendShutdown();
    // Release the application's real engine resources before deleting GL.
    backend::runtime_gl::ngReleaseOpenGLRenderBackend();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(nullptr);

    auto& state = detail::windowState();
    (void)SDL_GL_DestroyContext(state.gl_context);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
}

} // namespace nijigenerate::core
