// Port of independent nijilive/core/render/backends/opengl/shader_backend.d.
// Distributed under the project's BSD-2-Clause license.
#include "runtime_gl_shader.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <cstddef>
#include <string>
#include <utility>

#define NG_RUNTIME_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)
namespace nijigenerate::backend::runtime_gl {
namespace {
void checkShader(GLuint shader) {
    GLint status = 0;
    NG_RUNTIME_GL("glGetShaderiv", PFNGLGETSHADERIVPROC)(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length = 0;
        NG_RUNTIME_GL("glGetShaderiv", PFNGLGETSHADERIVPROC)(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            // D's new char[length] initializes every counted byte to char.init.
            std::string message(static_cast<std::size_t>(length), '\xff');
            NG_RUNTIME_GL("glGetShaderInfoLog", PFNGLGETSHADERINFOLOGPROC)(
                shader, length, nullptr, message.data());
            throw compat::ThrowableException(compat::RetainedString(std::move(message)),
                compat::RetainedString::fromStaticLiteral(
                    "nijilive/core/render/backends/opengl/shader_backend.d"), 26);
        }
        throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Shader compile failed"),
            compat::RetainedString::fromStaticLiteral(
                "nijilive/core/render/backends/opengl/shader_backend.d"), 28);
    }
}
void checkProgram(GLuint program) {
    GLint status = 0;
    NG_RUNTIME_GL("glGetProgramiv", PFNGLGETPROGRAMIVPROC)(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length = 0;
        NG_RUNTIME_GL("glGetProgramiv", PFNGLGETPROGRAMIVPROC)(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            std::string message(static_cast<std::size_t>(length), '\xff');
            NG_RUNTIME_GL("glGetProgramInfoLog", PFNGLGETPROGRAMINFOLOGPROC)(
                program, length, nullptr, message.data());
            throw compat::ThrowableException(compat::RetainedString(std::move(message)),
                compat::RetainedString::fromStaticLiteral(
                    "nijilive/core/render/backends/opengl/shader_backend.d"), 41);
        }
        throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Program link failed"),
            compat::RetainedString::fromStaticLiteral(
                "nijilive/core/render/backends/opengl/shader_backend.d"), 43);
    }
}
}

void oglCreateShaderProgram(ShaderProgramHandle& handle, std::string_view vertex, std::string_view fragment) {
    handle.vert = NG_RUNTIME_GL("glCreateShader", PFNGLCREATESHADERPROC)(GL_VERTEX_SHADER);
    const compat::SourceCString vertexText(vertex);
    const char* vsrc = vertexText.data();
    NG_RUNTIME_GL("glShaderSource", PFNGLSHADERSOURCEPROC)(handle.vert, 1, &vsrc, nullptr);
    NG_RUNTIME_GL("glCompileShader", PFNGLCOMPILESHADERPROC)(handle.vert);
    checkShader(handle.vert);

    handle.frag = NG_RUNTIME_GL("glCreateShader", PFNGLCREATESHADERPROC)(GL_FRAGMENT_SHADER);
    const compat::SourceCString fragmentText(fragment);
    const char* fsrc = fragmentText.data();
    NG_RUNTIME_GL("glShaderSource", PFNGLSHADERSOURCEPROC)(handle.frag, 1, &fsrc, nullptr);
    NG_RUNTIME_GL("glCompileShader", PFNGLCOMPILESHADERPROC)(handle.frag);
    checkShader(handle.frag);

    handle.program = NG_RUNTIME_GL("glCreateProgram", PFNGLCREATEPROGRAMPROC)();
    NG_RUNTIME_GL("glAttachShader", PFNGLATTACHSHADERPROC)(handle.program, handle.vert);
    NG_RUNTIME_GL("glAttachShader", PFNGLATTACHSHADERPROC)(handle.program, handle.frag);
    NG_RUNTIME_GL("glLinkProgram", PFNGLLINKPROGRAMPROC)(handle.program);
    checkProgram(handle.program);
}

void oglDestroyShaderProgram(ShaderProgramHandle& handle) {
    if (handle.program) {
        NG_RUNTIME_GL("glDetachShader", PFNGLDETACHSHADERPROC)(handle.program, handle.vert);
        NG_RUNTIME_GL("glDetachShader", PFNGLDETACHSHADERPROC)(handle.program, handle.frag);
        NG_RUNTIME_GL("glDeleteProgram", PFNGLDELETEPROGRAMPROC)(handle.program);
    }
    if (handle.vert) NG_RUNTIME_GL("glDeleteShader", PFNGLDELETESHADERPROC)(handle.vert);
    if (handle.frag) NG_RUNTIME_GL("glDeleteShader", PFNGLDELETESHADERPROC)(handle.frag);
    handle = {};
}

void oglUseShaderProgram(ShaderProgramHandle& handle) {
    NG_RUNTIME_GL("glUseProgram", PFNGLUSEPROGRAMPROC)(handle.program);
}

int oglShaderGetUniformLocation(ShaderProgramHandle& handle, std::string_view name) {
    const auto lookup = NG_RUNTIME_GL("glGetUniformLocation", PFNGLGETUNIFORMLOCATIONPROC);
    const auto program = handle.program;
    const compat::SourceCString terminated(name);
    return lookup(program, terminated.data());
}

void oglSetUniformBool(int location, bool value) {
    NG_RUNTIME_GL("glUniform1i", PFNGLUNIFORM1IPROC)(location, value ? 1 : 0);
}
void oglSetUniformInt(int location, int value) {
    NG_RUNTIME_GL("glUniform1i", PFNGLUNIFORM1IPROC)(location, value);
}
void oglSetUniformFloat(int location, float value) {
    NG_RUNTIME_GL("glUniform1f", PFNGLUNIFORM1FPROC)(location, value);
}
void oglSetUniformVec2(int location, nicxlive::core::math::Vec2 value) {
    NG_RUNTIME_GL("glUniform2f", PFNGLUNIFORM2FPROC)(location, value.x, value.y);
}
void oglSetUniformVec3(int location, nicxlive::core::math::Vec3 value) {
    NG_RUNTIME_GL("glUniform3f", PFNGLUNIFORM3FPROC)(location, value.x, value.y, value.z);
}
void oglSetUniformVec4(int location, nicxlive::core::math::Vec4 value) {
    NG_RUNTIME_GL("glUniform4f", PFNGLUNIFORM4FPROC)(location, value.x, value.y, value.z, value.w);
}
void oglSetUniformMat4(int location, nicxlive::core::math::Mat4 value) {
    NG_RUNTIME_GL("glUniformMatrix4fv", PFNGLUNIFORMMATRIX4FVPROC)(location, 1, GL_TRUE, &value[0][0]);
}
}
#undef NG_RUNTIME_GL
