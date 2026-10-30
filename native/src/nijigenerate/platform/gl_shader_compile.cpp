#include "nijigenerate/platform/gl_shader_compile.hpp"
#include "nijigenerate/platform/gl_font_texture.hpp"
#include "nijigenerate/platform/gl_device_delete.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#if !SDL_VERSION_ATLEAST(3, 2, 0)
#error "The GL procedure boundary requires the selected SDL 3.2 APIs"
#endif

namespace nijigenerate::platform {
namespace {

// The real drawable backend reads this binding through ngGLGetBackendProcedure.
constinit GLGenVertexArraysBindingState genVertexArraysBinding{};

#if !defined(_WIN32)
std::vector<char> allocateShaderLog(std::size_t count) {
    try {
        return std::vector<char>(count, static_cast<char>(0xff));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
#endif

template<class Function> Function loadGLProcedure(const char* name) {
    auto procedure = SDL_GL_GetProcAddress(name);
    if (!procedure) {
        throw std::runtime_error(std::string("Unavailable OpenGL procedure: ") + name);
    }
    static_assert(sizeof(Function) == sizeof(procedure));
    return std::bit_cast<Function>(procedure);
}

void requireShaderContext(unsigned minimumMajor = 2) {
    if (!SDL_IsMainThread()) throw std::logic_error("Shader inspection requires the SDL main thread");
    if (!SDL_GL_GetCurrentContext()) throw std::logic_error("Shader inspection requires a current SDL GL context");

    // GetString is core in any valid GL context. A nonnull extension address
    // alone is not a capability check, and GetError would drain caller errors.
    const auto getString = loadGLProcedure<decltype(&glGetString)>("glGetString");
    const auto* version = reinterpret_cast<const char*>(getString(GL_VERSION));
    if (!version) throw std::runtime_error("Current GL version is unavailable");
    bool hasDigits = false;
    unsigned major = 0;
    while (*version >= '0' && *version <= '9') {
        hasDigits = true;
        if (major < minimumMajor) {
            major = major * 10 + static_cast<unsigned>(*version - '0');
            if (major > minimumMajor) major = minimumMajor;
        }
        ++version;
    }
    if (!hasDigits || *version != '.' || major < minimumMajor) {
        throw std::runtime_error("Backend operation requires desktop OpenGL "
            + std::to_string(minimumMajor) + " or newer");
    }
}

} // namespace

GLGenVertexArraysBindingState ngGLGetGenVertexArraysBinding() noexcept {
    return genVertexArraysBinding;
}

void ngGLSetGenVertexArraysBinding(PFNGLGENVERTEXARRAYSPROC procedure) noexcept {
    genVertexArraysBinding = {procedure, true};
}

void ngGLRestoreGenVertexArraysBinding(GLGenVertexArraysBindingState state) noexcept {
    genVertexArraysBinding = state;
}

SDL_FunctionPointer ngGLGetBackendProcedure(const char* name) {
    if (std::strcmp(name, "glGenVertexArrays") == 0) {
        if (!genVertexArraysBinding.resolved) {
            requireShaderContext(3);
            const auto procedure = loadGLProcedure<PFNGLGENVERTEXARRAYSPROC>(name);
            genVertexArraysBinding = {procedure, true};
        }
        return std::bit_cast<SDL_FunctionPointer>(genVertexArraysBinding.procedure);
    }
    requireShaderContext(3);
    const auto procedure = SDL_GL_GetProcAddress(name);
    if (!procedure) throw std::runtime_error(std::string("Unavailable OpenGL procedure: ") + name);
    return procedure;
}

std::uint32_t ngGLBackendContextVersion() {
    requireShaderContext(3);
    const auto getString = loadGLProcedure<decltype(&glGetString)>("glGetString");
    const auto* text = reinterpret_cast<const char*>(getString(GL_VERSION));
    if (!text) throw std::runtime_error("Current GL version is unavailable");
    const auto component = [&text]() {
        unsigned value = 0;
        bool digits = false;
        while (*text >= '0' && *text <= '9') {
            digits = true;
            value = value * 10 + static_cast<unsigned>(*text++ - '0');
            if (value > 99) throw std::runtime_error("Unsupported desktop GL version component");
        }
        if (!digits) throw std::runtime_error("Invalid desktop GL version component");
        return value;
    };
    const auto major = component();
    if (*text++ != '.') throw std::runtime_error("Invalid desktop GL version separator");
    const auto minor = component();
    if (minor > 9) throw std::runtime_error("GL minor version cannot use the backend decimal encoding");
    return major * 100 + minor * 10;
}

bool ngGLCheckShaderCompile(std::uint32_t shader, const char* description) {
    static_assert(sizeof(GLuint) == sizeof(std::uint32_t) && std::is_unsigned_v<GLuint>);
    static_assert(sizeof(GLint) == sizeof(std::int32_t) && std::is_signed_v<GLint>);
    static_assert(std::numeric_limits<GLboolean>::digits == 8);
    requireShaderContext();
    GLint status = 0;
    GLint logLength = 0;
    const auto getShader = loadGLProcedure<PFNGLGETSHADERIVPROC>("glGetShaderiv");
    getShader(static_cast<GLuint>(shader), GL_COMPILE_STATUS, &status);
    getShader(static_cast<GLuint>(shader), GL_INFO_LOG_LENGTH, &logLength);

#if defined(_WIN32)
    (void)description;
#else
    if (static_cast<GLboolean>(status) == GL_FALSE) {
        std::fprintf(stderr,
            "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to compile %s!\n", description);
    }
    if (logLength > 1) {
        const auto signedLength = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(logLength) + 1u);
        auto buffer = allocateShaderLog(static_cast<std::size_t>(signedLength));
        const auto getLog = loadGLProcedure<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog");
        getLog(static_cast<GLuint>(shader), logLength, nullptr, buffer.data());
        std::fprintf(stderr, "%s\n", buffer.data());
    }
#endif
    return static_cast<GLboolean>(status) == GL_TRUE;
}

bool ngGLCheckProgramLink(std::uint32_t program, const char* description, const char* glslVersion) {
    requireShaderContext();
    GLint status = 0;
    GLint logLength = 0;
    const auto getProgram = loadGLProcedure<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
    getProgram(static_cast<GLuint>(program), GL_LINK_STATUS, &status);
    getProgram(static_cast<GLuint>(program), GL_INFO_LOG_LENGTH, &logLength);
#if defined(_WIN32)
    (void)description;
    (void)glslVersion;
#else
    if (static_cast<GLboolean>(status) == GL_FALSE) {
        std::fprintf(stderr, "ERROR: create_device_objects: failed to link %s! (with GLSL '%s')\n",
            description, glslVersion);
    }
    if (logLength > 1) {
        const auto signedLength = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(logLength) + 1u);
        auto buffer = allocateShaderLog(static_cast<std::size_t>(signedLength));
        const auto getLog = loadGLProcedure<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog");
        getLog(static_cast<GLuint>(program), logLength, nullptr, buffer.data());
        std::fprintf(stderr, "%s\n", buffer.data());
    }
#endif
    return static_cast<GLboolean>(status) == GL_TRUE;
}

std::int32_t ngGLUploadFontTexture(std::uint32_t& texture, const unsigned char* pixels, int width, int height) {
    requireShaderContext();
    const auto getInteger = loadGLProcedure<decltype(&glGetIntegerv)>("glGetIntegerv");
    const auto generate = loadGLProcedure<decltype(&glGenTextures)>("glGenTextures");
    const auto bind = loadGLProcedure<decltype(&glBindTexture)>("glBindTexture");
    const auto parameter = loadGLProcedure<decltype(&glTexParameteri)>("glTexParameteri");
    const auto upload = loadGLProcedure<decltype(&glTexImage2D)>("glTexImage2D");
    GLint previous = 0;
    getInteger(GL_TEXTURE_BINDING_2D, &previous);
    GLuint generated = texture;
    generate(1, &generated);
    texture = generated;
    bind(GL_TEXTURE_2D, generated);
    parameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    parameter(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    upload(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return previous;
}

void ngGLRestoreFontTextureBinding(std::int32_t binding) {
    requireShaderContext();
    loadGLProcedure<decltype(&glBindTexture)>("glBindTexture")(GL_TEXTURE_2D, static_cast<GLuint>(binding));
}

void ngGLDeleteFontTexture(std::uint32_t texture) {
    requireShaderContext();
    const GLuint name = texture;
    loadGLProcedure<decltype(&glDeleteTextures)>("glDeleteTextures")(1, &name);
}

void ngGLDeleteBackendBuffer(std::uint32_t buffer) {
    requireShaderContext();
    const GLuint name = buffer;
    loadGLProcedure<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers")(1, &name);
}

void ngGLDetachBackendShader(std::uint32_t program, std::uint32_t shader) {
    requireShaderContext();
    loadGLProcedure<PFNGLDETACHSHADERPROC>("glDetachShader")(program, shader);
}

void ngGLDeleteBackendShader(std::uint32_t shader) {
    requireShaderContext();
    loadGLProcedure<PFNGLDELETESHADERPROC>("glDeleteShader")(shader);
}

void ngGLDeleteBackendProgram(std::uint32_t program) {
    requireShaderContext();
    loadGLProcedure<PFNGLDELETEPROGRAMPROC>("glDeleteProgram")(program);
}

} // namespace nijigenerate::platform
