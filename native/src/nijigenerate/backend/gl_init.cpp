#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/backend/gl_state_internal.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"

namespace nijigenerate::backend::gl {

bool ngGLBackendInit(const char*) {
    auto& state = detail::glBackendState();
    // Preserve the original source constant, not a queried driver capability.
    constexpr GLint major = 4;
    constexpr GLint minor = 2;
    state.glVersion = static_cast<GLuint>(major * 100 + minor * 10);

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
