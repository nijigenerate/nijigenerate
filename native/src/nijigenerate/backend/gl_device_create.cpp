#include "nijigenerate/backend/gl.hpp"
#if defined(NIJIGENERATE_GL_DEBUG_PROVIDER)
#include "nijigenerate/backend/debug_gl.hpp"
#endif
#include "nijigenerate/backend/gl_state_internal.hpp"
#include "nijigenerate/backend/gl_shader_text_internal.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include <SDL3/SDL_opengl_glext.h>
#include <cstdio>

namespace nijigenerate::backend::gl {

bool ngGLBackendCreateDeviceObjects() {
    using platform::ngGLBackendProcedure;
    const auto getInteger = ngGLBackendProcedure<decltype(&glGetIntegerv)>("glGetIntegerv");
    const auto createShader = ngGLBackendProcedure<PFNGLCREATESHADERPROC>("glCreateShader");
    const auto shaderSource = ngGLBackendProcedure<PFNGLSHADERSOURCEPROC>("glShaderSource");
    const auto compileShader = ngGLBackendProcedure<PFNGLCOMPILESHADERPROC>("glCompileShader");
    const auto createProgram = ngGLBackendProcedure<PFNGLCREATEPROGRAMPROC>("glCreateProgram");
    const auto attachShader = ngGLBackendProcedure<PFNGLATTACHSHADERPROC>("glAttachShader");
    const auto linkProgram = ngGLBackendProcedure<PFNGLLINKPROGRAMPROC>("glLinkProgram");
    const auto getUniform = ngGLBackendProcedure<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
    const auto getAttribute = ngGLBackendProcedure<PFNGLGETATTRIBLOCATIONPROC>("glGetAttribLocation");
    const auto generateBuffers = ngGLBackendProcedure<PFNGLGENBUFFERSPROC>("glGenBuffers");
    const auto bindTexture = ngGLBackendProcedure<decltype(&glBindTexture)>("glBindTexture");
    const auto bindBuffer = ngGLBackendProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer");
    const auto bindVertexArray = ngGLBackendProcedure<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    GLint lastTexture = 0, lastArrayBuffer = 0, lastVertexArray = 0;
    getInteger(GL_TEXTURE_BINDING_2D, &lastTexture);
    getInteger(GL_ARRAY_BUFFER_BINDING, &lastArrayBuffer);
    getInteger(GL_VERTEX_ARRAY_BINDING, &lastVertexArray);
    auto& state = detail::glBackendState();
    int glslVersion = 130;
    std::sscanf(state.glslVersionString.data(), "#version %d", &glslVersion);
    const GLchar* vertexShader = nullptr;
    const GLchar* fragmentShader = nullptr;
    if (glslVersion < 130) {
        vertexShader = detail::vertex_shader_glsl_120;
        fragmentShader = detail::fragment_shader_glsl_120;
    } else if (glslVersion >= 410) {
        vertexShader = detail::vertex_shader_glsl_410_core;
        fragmentShader = detail::fragment_shader_glsl_410_core;
    } else if (glslVersion == 300) {
        vertexShader = detail::vertex_shader_glsl_300_es;
        fragmentShader = detail::fragment_shader_glsl_300_es;
    } else {
        vertexShader = detail::vertex_shader_glsl_130;
        fragmentShader = detail::fragment_shader_glsl_130;
    }
    const GLchar* vertexWithVersion[2] = {state.glslVersionString.data(), vertexShader};
    state.vertexShaderHandle = createShader(GL_VERTEX_SHADER);
    shaderSource(state.vertexShaderHandle, 2, vertexWithVersion, nullptr);
    compileShader(state.vertexShaderHandle);
    ngGLBackendCheckShader(state.vertexShaderHandle, "vertex shader");
    const GLchar* fragmentWithVersion[2] = {state.glslVersionString.data(), fragmentShader};
    state.fragmentShaderHandle = createShader(GL_FRAGMENT_SHADER);
    shaderSource(state.fragmentShaderHandle, 2, fragmentWithVersion, nullptr);
    compileShader(state.fragmentShaderHandle);
    ngGLBackendCheckShader(state.fragmentShaderHandle, "fragment shader");
    state.shaderHandle = createProgram();
    attachShader(state.shaderHandle, state.vertexShaderHandle);
    attachShader(state.shaderHandle, state.fragmentShaderHandle);
    linkProgram(state.shaderHandle);
    ngGLBackendCheckProgram(state.shaderHandle, "shader program");
    state.attributeTexture = getUniform(state.shaderHandle, "Texture");
    state.attributeProjectionMatrix = getUniform(state.shaderHandle, "ProjMtx");
    state.attributeVertexPosition = static_cast<GLuint>(getAttribute(state.shaderHandle, "Position"));
    state.attributeVertexUV = static_cast<GLuint>(getAttribute(state.shaderHandle, "UV"));
    state.attributeVertexColor = static_cast<GLuint>(getAttribute(state.shaderHandle, "Color"));
    generateBuffers(1, &state.vertexBufferHandle);
    generateBuffers(1, &state.elementsHandle);
    ngGLBackendCreateFontsTexture();
    bindTexture(GL_TEXTURE_2D, static_cast<GLuint>(lastTexture));
    bindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(lastArrayBuffer));
    bindVertexArray(static_cast<GLuint>(lastVertexArray));
#if defined(NIJIGENERATE_GL_DEBUG_PROVIDER)
    debug_gl::ngInstallOpenGLDebugProvider();
#endif
    return true;
}

} // namespace nijigenerate::backend::gl
