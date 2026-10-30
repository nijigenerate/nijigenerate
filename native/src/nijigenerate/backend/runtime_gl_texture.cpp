// Complete independent OpenGL texture_backend.d production source, BSD-2-Clause.
#include "runtime_gl_texture.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <limits>
#include <string>
#include <utility>

#define NG_TEXTURE_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)

namespace nijigenerate::backend::runtime_gl {
#ifndef NICXLIVE_SOURCE_NO_RENDER
namespace {
GLuint channelFormat(int channels) {
    switch (channels) {
    case 1: return GL_RED;
    case 2: return GL_RG;
    case 3: return GL_RGB;
    default: return GL_RGBA;
    }
}
void bind(GLId id) {
    NG_TEXTURE_GL("glBindTexture", void(APIENTRY*)(GLenum, GLuint))(GL_TEXTURE_2D, id);
}
}
void oglCreateTextureHandle(GLId& id) {
    GLuint handle = 0;
    NG_TEXTURE_GL("glGenTextures", void(APIENTRY*)(GLsizei, GLuint*))(1, &handle);
    if (handle == 0) {
        throw compat::ThrowableException(
            compat::RetainedString(std::string("Failed to create texture")),
            compat::RetainedString::fromStaticLiteral(
                "nijilive/core/render/backends/opengl/texture_backend.d"), 42);
    }
    id = handle;
}
void oglDeleteTextureHandle(GLId& id) {
    if (id != 0) {
        GLuint handle = id;
        NG_TEXTURE_GL("glDeleteTextures", void(APIENTRY*)(GLsizei, const GLuint*))(1, &handle);
        id = 0;
    }
}
void oglBindTextureHandle(GLId id, unsigned unit) {
    NG_TEXTURE_GL("glActiveTexture", PFNGLACTIVETEXTUREPROC)(GL_TEXTURE0 + (unit <= 31 ? unit : 31));
    bind(id);
}
template<class Bytes>
static void uploadTextureDataImpl(GLId id, int width, int height, int inChannels, int outChannels,
    bool stencil, Bytes data) {
    bind(id);
    if (stencil) {
        NG_TEXTURE_GL("glTexImage2D",
            void(APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*))(
                GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0,
                GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        return;
    }
    NG_TEXTURE_GL("glPixelStorei", void(APIENTRY*)(GLenum, GLint))(GL_UNPACK_ALIGNMENT, 1);
    NG_TEXTURE_GL("glPixelStorei", void(APIENTRY*)(GLenum, GLint))(GL_PACK_ALIGNMENT, 1);
    const auto inFormat = channelFormat(inChannels);
    const auto outFormat = channelFormat(outChannels);
    NG_TEXTURE_GL("glTexImage2D",
        void(APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*))(
            GL_TEXTURE_2D, 0, outFormat, width, height, 0, inFormat, GL_UNSIGNED_BYTE, data.data());
}
template<class Bytes>
static void updateTextureRegionImpl(GLId id, int x, int y, int width, int height, int channels,
    Bytes data) {
    const auto format = channelFormat(channels);
    bind(id);
    NG_TEXTURE_GL("glTexSubImage2D",
        void(APIENTRY*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*))(
            GL_TEXTURE_2D, 0, x, y, width, height, format, GL_UNSIGNED_BYTE, data.data());
}
void oglGenerateTextureMipmap(GLId id) {
    bind(id);
    NG_TEXTURE_GL("glGenerateMipmap", PFNGLGENERATEMIPMAPPROC)(GL_TEXTURE_2D);
}
void oglApplyTextureFiltering(GLId id, nicxlive::core::Filtering filtering, bool useMipmaps) {
    bind(id);
    const bool linear = filtering == nicxlive::core::Filtering::Linear;
    const auto minFilter = useMipmaps ? (linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
        : (linear ? GL_LINEAR : GL_NEAREST);
    const auto magFilter = linear ? GL_LINEAR : GL_NEAREST;
    NG_TEXTURE_GL("glTexParameteri", void(APIENTRY*)(GLenum, GLenum, GLint))(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    NG_TEXTURE_GL("glTexParameteri", void(APIENTRY*)(GLenum, GLenum, GLint))(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
}
void oglApplyTextureWrapping(GLId id, nicxlive::core::Wrapping wrapping) {
    bind(id);
    GLint value;
    switch (wrapping) {
    case nicxlive::core::Wrapping::Clamp: value = GL_CLAMP_TO_BORDER; break;
    case nicxlive::core::Wrapping::Repeat: value = GL_REPEAT; break;
    case nicxlive::core::Wrapping::Mirror: value = GL_MIRRORED_REPEAT; break;
    default: value = GL_CLAMP_TO_BORDER; break;
    }
    NG_TEXTURE_GL("glTexParameteri", void(APIENTRY*)(GLenum, GLenum, GLint))(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, value);
    NG_TEXTURE_GL("glTexParameteri", void(APIENTRY*)(GLenum, GLenum, GLint))(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, value);
    if (wrapping == nicxlive::core::Wrapping::Clamp) {
        const GLfloat border[4] = {0, 0, 0, 0};
        NG_TEXTURE_GL("glTexParameterfv", void(APIENTRY*)(GLenum, GLenum, const GLfloat*))(
            GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    }
}
void oglApplyTextureAnisotropy(GLId id, float value) {
    bind(id);
    NG_TEXTURE_GL("glTexParameterf", void(APIENTRY*)(GLenum, GLenum, GLfloat))(
        GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, value);
}
float oglMaxTextureAnisotropy() {
    float value = std::numeric_limits<float>::quiet_NaN();
    NG_TEXTURE_GL("glGetFloatv", void(APIENTRY*)(GLenum, GLfloat*))(GL_MAX_TEXTURE_MAX_ANISOTROPY, &value);
    return value;
}
template<class Bytes>
static void readTextureDataImpl(GLId id, int channels, bool stencil, Bytes buffer) {
    bind(id);
    const auto format = stencil ? GL_DEPTH_STENCIL : channelFormat(channels);
    NG_TEXTURE_GL("glGetTexImage", void(APIENTRY*)(GLenum, GLint, GLenum, GLenum, void*))(
        GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, buffer.data());
}

// Both the native borrowed API and the real Source descriptor use these same kernels.
void oglUploadTextureData(GLId id, int width, int height, int inChannels, int outChannels,
    bool stencil, std::span<const std::uint8_t> data) {
    uploadTextureDataImpl(id, width, height, inChannels, outChannels, stencil, data);
}
void oglUploadTextureData(GLId id, int width, int height, int inChannels, int outChannels,
    bool stencil, nicxlive::core::SourceTextureBytes data) {
    uploadTextureDataImpl(id, width, height, inChannels, outChannels, stencil, std::move(data));
}
void oglUpdateTextureRegion(GLId id, int x, int y, int width, int height, int channels,
    std::span<const std::uint8_t> data) {
    updateTextureRegionImpl(id, x, y, width, height, channels, data);
}
void oglUpdateTextureRegion(GLId id, int x, int y, int width, int height, int channels,
    nicxlive::core::SourceTextureBytes data) {
    updateTextureRegionImpl(id, x, y, width, height, channels, std::move(data));
}
void oglReadTextureData(GLId id, int channels, bool stencil, std::span<std::uint8_t> buffer) {
    readTextureDataImpl(id, channels, stencil, buffer);
}
void oglReadTextureData(GLId id, int channels, bool stencil, nicxlive::core::SourceTextureBytes buffer) {
    readTextureDataImpl(id, channels, stencil, std::move(buffer));
}
#else
// The existing original's non-rendering branch, not an enabled-backend fallback.
void oglCreateTextureHandle(GLId& id) { id = 0; }
void oglDeleteTextureHandle(GLId& id) { id = 0; }
void oglBindTextureHandle(GLId, unsigned) {}
void oglUploadTextureData(GLId, int, int, int, int, bool, std::span<const std::uint8_t>) {}
void oglUploadTextureData(GLId, int, int, int, int, bool, nicxlive::core::SourceTextureBytes) {}
void oglUpdateTextureRegion(GLId, int, int, int, int, int, std::span<const std::uint8_t>) {}
void oglUpdateTextureRegion(GLId, int, int, int, int, int, nicxlive::core::SourceTextureBytes) {}
void oglGenerateTextureMipmap(GLId) {}
void oglApplyTextureFiltering(GLId, nicxlive::core::Filtering, bool) {}
void oglApplyTextureWrapping(GLId, nicxlive::core::Wrapping) {}
void oglApplyTextureAnisotropy(GLId, float) {}
float oglMaxTextureAnisotropy() { return 1.0f; }
void oglReadTextureData(GLId, int, bool, std::span<std::uint8_t>) {}
void oglReadTextureData(GLId, int, bool, nicxlive::core::SourceTextureBytes) {}
#endif
}
#undef NG_TEXTURE_GL
