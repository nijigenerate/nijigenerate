#include "runtime_gl_dynamic_composite.hpp"
#include "runtime_gl_runtime.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include <runtime_state.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <stdexcept>
#if defined(NIJILIVE_ENABLE_DEBUG_LOG) && defined(NIJILIVE_DEBUG_RENDER_PROFILER)
#include "nijigenerate/core/typed_format.hpp"
#include <cstdio>
#endif
#if defined(NIJILIVE_RENDER_PROFILER)
#include <render/profiler.hpp>
#include <chrono>
#endif

#define NG_COMPOSITE_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)
namespace nijigenerate::backend::runtime_gl {
namespace {
#if defined(NIJILIVE_ENABLE_DEBUG_LOG) && defined(NIJILIVE_DEBUG_RENDER_PROFILER)
template<class... Args> void compositeLog(std::string_view pattern, Args&&... args) {
    using namespace core::translation;
    const std::array<TypedFormatArgument, sizeof...(Args)> arguments{ngFormatArgument(std::forward<Args>(args))...};
    const auto message = ngFormatTyped(pattern, arguments) + '\n';
    if (std::fwrite(message.data(), 1, message.size(), stdout) != message.size())
        throw std::runtime_error("Could not write dynamic composite diagnostic");
}
std::string vectorText(nicxlive::core::math::Vec2 value) {
    using namespace core::translation;
    const std::array<TypedFormatArgument, 2> arguments{ngFormatArgument(value.x), ngFormatArgument(value.y)};
    return ngFormatTyped("[%s, %s]", arguments);
}
#define NG_COMPOSITE_LOG(...) compositeLog(__VA_ARGS__)
#else
#define NG_COMPOSITE_LOG(...) ((void)0)
#endif
#if defined(NIJILIVE_RENDER_PROFILER)
// Accumulators were __gshared; timing state was ordinary D module TLS.
std::uint64_t cpuAccumUsec = 0;
std::uint64_t gpuAccumUsec = 0;
thread_local GLuint compositeTimeQuery = 0;
thread_local bool compositeTimerInit = false;
thread_local bool compositeTimerActive = false;
thread_local std::chrono::steady_clock::time_point compositeCpuStart;
thread_local bool compositeCpuActive = false;
void ensureCompositeTimer() {
    if (compositeTimerInit) return;
    compositeTimerInit = true;
    NG_COMPOSITE_GL("glGenQueries", PFNGLGENQUERIESPROC)(1, &compositeTimeQuery);
}
#endif
GLuint textureId(const std::shared_ptr<nicxlive::core::Texture>& texture) {
    return texture ? texture->backendId() : 0;
}
}
#if defined(NIJILIVE_RENDER_PROFILER)
void resetCompositeAccum() { cpuAccumUsec = 0; gpuAccumUsec = 0; }
std::uint64_t compositeCpuAccumUsec() { return cpuAccumUsec; }
std::uint64_t compositeGpuAccumUsec() { return gpuAccumUsec; }
#endif
void oglBeginDynamicComposite(nicxlive::core::DynamicCompositePass& pass) {
    auto surface = pass.surface;
    if (!surface) {
        NG_COMPOSITE_LOG("[nijilive] oglBeginDynamicComposite skip: surface=null");
        return;
    }
    if (surface->textureCount == 0) {
        NG_COMPOSITE_LOG("[nijilive] oglBeginDynamicComposite skip: textureCount=0");
        return;
    }
    auto texture = surface->textures[0];
    if (!texture) {
        NG_COMPOSITE_LOG("[nijilive] oglBeginDynamicComposite skip: tex[0]=null");
        return;
    }
    if (surface->framebuffer == 0) {
        GLuint framebuffer = 0;
        NG_COMPOSITE_GL("glGenFramebuffers", PFNGLGENFRAMEBUFFERSPROC)(1, &framebuffer);
        surface->framebuffer = framebuffer;
    }
    GLint previousFramebuffer = 0;
    auto getInteger = NG_COMPOSITE_GL("glGetIntegerv", void(APIENTRY*)(GLenum, GLint*));
    getInteger(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
    pass.origBuffer = static_cast<unsigned>(previousFramebuffer);
    getInteger(GL_VIEWPORT, pass.origViewport);
    NG_COMPOSITE_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)(GL_FRAMEBUFFER, surface->framebuffer);
    std::array<GLuint, 3> drawBuffers{};
    std::size_t count = 0;
    auto attach = NG_COMPOSITE_GL("glFramebufferTexture2D", PFNGLFRAMEBUFFERTEXTURE2DPROC);
    for (std::size_t i = 0; i < surface->textureCount; ++i) {
        const auto attachment = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
        auto attachmentTexture = surface->textures.at(i);
        if (attachmentTexture) {
            attach(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, textureId(attachmentTexture), 0);
            drawBuffers.at(count++) = attachment;
        } else {
            attach(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
        }
    }
    if (count == 0) drawBuffers[count++] = GL_COLOR_ATTACHMENT0;
    auto clear = NG_COMPOSITE_GL("glClear", void(APIENTRY*)(GLbitfield));
    if (surface->stencil) {
        attach(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textureId(surface->stencil), 0);
        clear(GL_STENCIL_BUFFER_BIT);
    } else {
        attach(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    }
    nicxlive::core::inPushViewport(texture->width(), texture->height());
    auto camera = nicxlive::core::inGetCamera();
    camera.scale = {1, -1};
    const float inverseX = pass.scale.x == 0 ? 0 : 1 / pass.scale.x;
    const float inverseY = pass.scale.y == 0 ? 0 : 1 / pass.scale.y;
    using nicxlive::core::math::Mat4;
    const auto scaling = Mat4::scale({inverseX, inverseY, 1});
    // LDC Windows MSVC D real is binary64; zRotation evaluates double trig before narrowing.
    const auto rotation = Mat4::zRotation(-pass.rotationZ) * Mat4::identity();
    const auto offset = scaling * rotation;
    // Preserve the original matrix/vector operation, including signed zeros and non-finite inputs.
    const float vector[4] = {-0.0f, -0.0f, -0.0f, -1.0f};
    float position[2]{};
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 4; ++column) {
            position[row] = nicxlive::core::math::detail::inmathAccumulate(position[row],
                nicxlive::core::math::detail::inmathProduct(offset[row][column], vector[column]));
        }
    }
    camera.position = {position[0], position[1]};
    nicxlive::core::inSetCamera(camera);
    NG_COMPOSITE_GL("glDrawBuffers", PFNGLDRAWBUFFERSPROC)(static_cast<int>(count), drawBuffers.data());
    NG_COMPOSITE_GL("glViewport", void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei))(
        0, 0, texture->width(), texture->height());
    NG_COMPOSITE_GL("glClearColor", void(APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat))(0, 0, 0, 0);
    clear(GL_COLOR_BUFFER_BIT);
    NG_COMPOSITE_GL("glActiveTexture", PFNGLACTIVETEXTUREPROC)(GL_TEXTURE0);
    NG_COMPOSITE_GL("glBlendFunc", void(APIENTRY*)(GLenum, GLenum))(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    NG_COMPOSITE_LOG("[nijilive] oglBeginDynamicComposite fbo=%s tex0=%s size=%sx%s scale=%s rotZ=%s autoScaled=%s origFbo=%s origViewport=%s,%s,%s,%s cameraPos=%s cameraScale=%s cameraRot=%s",
        surface->framebuffer, textureId(texture), texture->width(), texture->height(), vectorText(pass.scale),
        pass.rotationZ, pass.autoScaled, pass.origBuffer, pass.origViewport[0], pass.origViewport[1],
        pass.origViewport[2], pass.origViewport[3], vectorText(camera.position), vectorText(camera.scale), camera.rotation);
#if defined(NIJILIVE_RENDER_PROFILER)
    if (!compositeCpuActive) {
        compositeCpuActive = true;
        compositeCpuStart = std::chrono::steady_clock::now();
    }
    ensureCompositeTimer();
    if (!compositeTimerActive && compositeTimeQuery != 0) {
        NG_COMPOSITE_GL("glBeginQuery", PFNGLBEGINQUERYPROC)(GL_TIME_ELAPSED, compositeTimeQuery);
        compositeTimerActive = true;
    }
#endif
}
void oglEndDynamicComposite(nicxlive::core::DynamicCompositePass& pass) {
    if (!pass.surface) {
        NG_COMPOSITE_LOG("[nijilive] oglEndDynamicComposite skip: surface=null");
        return;
    }
    oglRebindActiveTargets();
    NG_COMPOSITE_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)(GL_FRAMEBUFFER, pass.origBuffer);
    nicxlive::core::inPopViewport();
    NG_COMPOSITE_GL("glViewport", void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei))(
        pass.origViewport[0], pass.origViewport[1], pass.origViewport[2], pass.origViewport[3]);
    const GLenum attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    NG_COMPOSITE_GL("glDrawBuffers", PFNGLDRAWBUFFERSPROC)(3, attachments);
    NG_COMPOSITE_LOG("[nijilive] oglEndDynamicComposite restore origFbo=%s viewport=%s,%s,%s,%s autoScaled=%s",
        pass.origBuffer, pass.origViewport[0], pass.origViewport[1], pass.origViewport[2], pass.origViewport[3],
        pass.autoScaled);
#if defined(NIJILIVE_RENDER_PROFILER)
    if (compositeTimerActive && compositeTimeQuery != 0) {
        NG_COMPOSITE_GL("glEndQuery", PFNGLENDQUERYPROC)(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        NG_COMPOSITE_GL("glGetQueryObjectui64v", PFNGLGETQUERYOBJECTUI64VPROC)(
            compositeTimeQuery, GL_QUERY_RESULT, &ns);
        nicxlive::core::render::renderProfilerAddSampleUsec("Composite.Offscreen", ns / 1000);
        gpuAccumUsec += ns / 1000;
        compositeTimerActive = false;
    }
    if (compositeCpuActive) {
        const auto duration = std::chrono::steady_clock::now() - compositeCpuStart;
        const auto usec = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        nicxlive::core::render::renderProfilerAddSampleUsec("Composite.Offscreen.CPU", usec);
        cpuAccumUsec += usec;
        compositeCpuActive = false;
    }
#endif
    NG_COMPOSITE_GL("glFlush", void(APIENTRY*)())();
    auto texture = pass.surface->textureCount > 0 ? pass.surface->textures[0] : nullptr;
    if (texture && !pass.autoScaled) texture->genMipmap();
}
void oglBeginDynamicComposite(nicxlive::core::DynamicCompositePass* pass) {
    if (!pass) {
        NG_COMPOSITE_LOG("[nijilive] oglBeginDynamicComposite skip: pass=null");
        return;
    }
    oglBeginDynamicComposite(*pass);
}
void oglEndDynamicComposite(nicxlive::core::DynamicCompositePass* pass) {
    if (!pass) {
        NG_COMPOSITE_LOG("[nijilive] oglEndDynamicComposite skip: pass=null");
        return;
    }
    oglEndDynamicComposite(*pass);
}
void oglDestroyDynamicComposite(const std::shared_ptr<nicxlive::core::DynamicCompositeSurface>& surface) {
    if (!surface) return;
    if (surface->framebuffer != 0) {
        GLuint framebuffer = surface->framebuffer;
        NG_COMPOSITE_GL("glDeleteFramebuffers", PFNGLDELETEFRAMEBUFFERSPROC)(1, &framebuffer);
        surface->framebuffer = 0;
    }
}
}
#undef NG_COMPOSITE_GL
#undef NG_COMPOSITE_LOG
