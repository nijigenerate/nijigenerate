#include "nijigenerate/backend/gl.hpp"
#if defined(NIJIGENERATE_GL_DEBUG_PROVIDER)
#include "nijigenerate/backend/debug_gl.hpp"
#endif
#include "nijigenerate/backend/gl_state_internal.hpp"
#include "nijigenerate/platform/gl_device_delete.hpp"

namespace nijigenerate::backend::gl {

void ngGLBackendDestroyDeviceObjects() {
#if defined(NIJIGENERATE_GL_DEBUG_PROVIDER)
    debug_gl::ngReleaseOpenGLDebugProvider();
#endif
    auto& state = detail::glBackendState();
    if (state.vertexBufferHandle) {
        platform::ngGLDeleteBackendBuffer(state.vertexBufferHandle);
        state.vertexBufferHandle = 0;
    }
    if (state.elementsHandle) {
        platform::ngGLDeleteBackendBuffer(state.elementsHandle);
        state.elementsHandle = 0;
    }
    if (state.shaderHandle && state.vertexShaderHandle) {
        platform::ngGLDetachBackendShader(state.shaderHandle, state.vertexShaderHandle);
    }
    if (state.shaderHandle && state.fragmentShaderHandle) {
        platform::ngGLDetachBackendShader(state.shaderHandle, state.fragmentShaderHandle);
    }
    if (state.vertexShaderHandle) {
        platform::ngGLDeleteBackendShader(state.vertexShaderHandle);
        state.vertexShaderHandle = 0;
    }
    if (state.fragmentShaderHandle) {
        platform::ngGLDeleteBackendShader(state.fragmentShaderHandle);
        state.fragmentShaderHandle = 0;
    }
    if (state.shaderHandle) {
        platform::ngGLDeleteBackendProgram(state.shaderHandle);
        state.shaderHandle = 0;
    }
    ngGLBackendDestroyFontsTexture();
}

} // namespace nijigenerate::backend::gl
