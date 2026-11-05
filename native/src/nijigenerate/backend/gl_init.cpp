#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/backend/gl_state_internal.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"

namespace nijigenerate::backend::gl {

bool ngGLBackendInit(const char*) {
    auto& state = detail::glBackendState();
    // The window may supply a 3.2 or 3.1 context. Only advertise capabilities
    // provided by that context, including sampler objects introduced in 3.3.
    state.glVersion = platform::ngGLBackendContextVersion();

    auto& io = ImGui::GetIO();
    if (state.glVersion >= 320) {
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    }
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    using GetInteger = void (APIENTRY*)(GLenum, GLint*);
    const auto getInteger = platform::ngGLBackendProcedure<GetInteger>("glGetIntegerv");
    GLint currentTexture;
    getInteger(GL_TEXTURE_BINDING_2D, &currentTexture);

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ngGLBackendPlatformInterfaceInit();
    }
    return true;
}

} // namespace nijigenerate::backend::gl
