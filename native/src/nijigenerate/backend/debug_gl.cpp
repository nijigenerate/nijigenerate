// Port of nijilive OpenGL debug_renderer.d, BSD-2-Clause.
#include "debug_gl.hpp"
#include "runtime_gl_backend.hpp"
#include "runtime_gl_soa_upload.hpp"
#include "debug_gl_shaders.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <shader.hpp>
#include <compat/native_access_fault.hpp>
#include <render/profiler.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <SDL3/SDL_video.h>
#include <runtime_state.hpp>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <limits>
#include <stdexcept>
#include <vector>

#define NG_DEBUG_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)

namespace nijigenerate::backend::debug_gl {
namespace {
using namespace core::dbg;
using nicxlive::core::Shader;
using nicxlive::core::d_runtime::RetainedString;
// These two Source __gshared fields keep their existing process-wide storage.
std::atomic<int> pointCount{0};
std::atomic<bool> bufferIsSoA{false};

void requireOwnerContext();
class OpenGLDebugProvider final : public DebugProvider {
    std::shared_ptr<Shader> lineShader, pointShader;
    GLuint vao = 0, vbo = 0, ibo = 0, currentVbo = 0;
    int indexCount = 0;
    int lineMvpLocation = -1, lineColorLocation = -1;
    int pointMvpLocation = -1, pointColorLocation = -1;
public:
    void initialize() override {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        requireOwnerContext();
        // The Source guard is the actual line Shader object, not an extra flag.
        if (lineShader) return;
        lineShader = std::make_shared<Shader>(nicxlive::core::shaderAsset<
            detail::vertexShaderSource, detail::lineShaderSource>());
        pointShader = std::make_shared<Shader>(nicxlive::core::shaderAsset<
            detail::vertexShaderSource, detail::pointShaderSource>());
        // Preserve partial initialization if a later construction or lookup throws.
        lineMvpLocation = nicxlive::core::d_runtime::ngRequireObject(lineShader)->getUniformLocation(
            RetainedString::fromStaticLiteral("mvp"));
        lineColorLocation = nicxlive::core::d_runtime::ngRequireObject(lineShader)->getUniformLocation(
            RetainedString::fromStaticLiteral("color"));
        pointMvpLocation = nicxlive::core::d_runtime::ngRequireObject(pointShader)->getUniformLocation(
            RetainedString::fromStaticLiteral("mvp"));
        pointColorLocation = nicxlive::core::d_runtime::ngRequireObject(pointShader)->getUniformLocation(
            RetainedString::fromStaticLiteral("color"));
        NG_DEBUG_GL("glGenVertexArrays", PFNGLGENVERTEXARRAYSPROC)(1, &vao);
        NG_DEBUG_GL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &vbo);
        NG_DEBUG_GL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &ibo);
        currentVbo = vbo;
        indexCount = 0;
#endif
    }
    // Existing explicit native GL lifecycle release; ordinary Shader ownership.
    void release() {
        requireOwnerContext();
        if (vao) NG_DEBUG_GL("glDeleteVertexArrays", PFNGLDELETEVERTEXARRAYSPROC)(1, &vao);
        if (vbo) NG_DEBUG_GL("glDeleteBuffers", PFNGLDELETEBUFFERSPROC)(1, &vbo);
        if (ibo) NG_DEBUG_GL("glDeleteBuffers", PFNGLDELETEBUFFERSPROC)(1, &ibo);
        lineShader.reset(); pointShader.reset();
        vao = vbo = ibo = currentVbo = 0;
        indexCount = 0;
        lineMvpLocation = lineColorLocation = pointMvpLocation = pointColorLocation = -1;
        pointCount.store(0, std::memory_order_relaxed);
        bufferIsSoA.store(false, std::memory_order_relaxed);
    }
    void pointSize(float size) override {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        requireOwnerContext();
        NG_DEBUG_GL("glPointSize", decltype(&glPointSize))(size);
#else
        (void)size;
#endif
    }
    void lineWidth(float size) override {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        requireOwnerContext();
        NG_DEBUG_GL("glLineWidth", decltype(&glLineWidth))(size);
#else
        (void)size;
#endif
    }
    void upload(const Vec3Array& positions, std::span<const std::uint16_t> indices) override {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        initialize();
        if (positions.empty() || indices.empty()) {
            indexCount = 0;
            pointCount.store(0, std::memory_order_relaxed);
            bufferIsSoA.store(false, std::memory_order_relaxed);
            return;
        }
        NG_DEBUG_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(vao);
        runtime_gl::glUploadFloatVecArray(vbo, positions, GL_DYNAMIC_DRAW, "UploadDebug");
        currentVbo = vbo;
        pointCount.store(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(positions.size())),
            std::memory_order_relaxed);
        bufferIsSoA.store(true, std::memory_order_relaxed);
        NG_DEBUG_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, ibo);
        NG_DEBUG_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size_bytes()), indices.data(), GL_DYNAMIC_DRAW);
        indexCount = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(indices.size()));
#else
        (void)positions; (void)indices;
#endif
    }
    void external(RenderResourceHandle vertices, RenderResourceHandle indices, int count) override {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        initialize();
        const auto vertexHandle = static_cast<GLuint>(vertices);
        const auto indexHandle = static_cast<GLuint>(indices);
        NG_DEBUG_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(vao);
        NG_DEBUG_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, vertexHandle);
        NG_DEBUG_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, indexHandle);
        currentVbo = vertexHandle;
        indexCount = count;
        bufferIsSoA.store(false, std::memory_order_relaxed);
        pointCount.store(0, std::memory_order_relaxed);
#else
        (void)vertices; (void)indices; (void)count;
#endif
    }
private:
    void prepare(std::shared_ptr<Shader> shader, int mvpLocation, int colorLocation, Mat4 matrix, Vec4 color) {
        if (!shader || indexCount <= 0) return;
        shader->use();
        shader->setUniform(mvpLocation, matrix);
        shader->setUniform(colorLocation, color);
        NG_DEBUG_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(vao);
        if (bufferIsSoA.load(std::memory_order_relaxed) && pointCount.load(std::memory_order_relaxed) > 0) {
            const auto laneBytes = static_cast<std::uintptr_t>(
                static_cast<std::ptrdiff_t>(pointCount.load(std::memory_order_relaxed))) * sizeof(float);
            for (GLuint lane = 0; lane < 3; ++lane) {
                NG_DEBUG_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(lane);
                NG_DEBUG_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, currentVbo);
                NG_DEBUG_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(lane, 1, GL_FLOAT,
                    GL_FALSE, 0, reinterpret_cast<const void*>(laneBytes * lane));
            }
        } else {
            NG_DEBUG_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(0);
            NG_DEBUG_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, currentVbo);
            NG_DEBUG_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
            NG_DEBUG_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(1);
            NG_DEBUG_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(2);
        }
    }
    void finish() {
        NG_DEBUG_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(0);
        NG_DEBUG_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(1);
        NG_DEBUG_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(2);
        NG_DEBUG_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(0);
    }
    void draw(Vec4 color, Mat4 matrix, bool drawLines) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
        initialize();
        if (indexCount <= 0) return;
        if (drawLines) NG_DEBUG_GL("glEnable", decltype(&glEnable))(GL_LINE_SMOOTH);
        NG_DEBUG_GL("glBlendEquation", PFNGLBLENDEQUATIONPROC)(GL_FUNC_ADD);
        NG_DEBUG_GL("glBlendFuncSeparate", PFNGLBLENDFUNCSEPARATEPROC)(
            GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
        if (drawLines) prepare(lineShader, lineMvpLocation, lineColorLocation, matrix, color);
        else prepare(pointShader, pointMvpLocation, pointColorLocation, matrix, color);
        NG_DEBUG_GL("glDrawElements", decltype(&glDrawElements))(
            drawLines ? GL_LINES : GL_POINTS, indexCount, GL_UNSIGNED_SHORT, nullptr);
        finish();
        NG_DEBUG_GL("glBlendFuncSeparate", PFNGLBLENDFUNCSEPARATEPROC)(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
        if (drawLines) NG_DEBUG_GL("glDisable", decltype(&glDisable))(GL_LINE_SMOOTH);
#else
        (void)color; (void)matrix; (void)drawLines;
#endif
    }
public:
    void points(Vec4 color, const Mat4& matrix) override { draw(color, matrix, false); }
    void lines(Vec4 color, const Mat4& matrix) override { draw(color, matrix, true); }
};

thread_local std::shared_ptr<OpenGLDebugProvider> current;
thread_local SDL_GLContext ownerContext = nullptr;
// This weak identity survives ImGui device-object recreation, but does not own
// the renderer. Only explicit selection of a concrete GL backend establishes it.
thread_local std::weak_ptr<nicxlive::core::RenderBackend> boundRuntime;
void requireOwnerContext() {
    if (!ownerContext || SDL_GL_GetCurrentContext() != ownerContext)
        throw std::logic_error("GL debug resources require their owning current context");
}
void restoreBinding() {
    const auto backend = boundRuntime.lock();
    if (backend && backend == nicxlive::core::getCurrentRenderBackend() &&
        std::dynamic_pointer_cast<runtime_gl::OpenGLRenderBackend>(backend)) {
        core::dbg::ngInstallDebugProvider(current, backend);
    } else {
        boundRuntime.reset();
    }
}
}
void ngInstallOpenGLDebugProvider() {
    // Runtime initialization precedes ImGui device creation in the source.
    // Repeated installation in that same context must retain its GL objects.
    if (current) {
        requireOwnerContext();
        return;
    }
    const auto context = SDL_GL_GetCurrentContext();
    if (!context) throw std::logic_error("GL debug provider requires a current context");
    ownerContext = context;
    current = std::make_shared<OpenGLDebugProvider>();
    restoreBinding();
}
std::shared_ptr<core::dbg::DebugProvider> ngOpenGLDebugProvider() {
    if (!current) throw std::logic_error("GL debug provider has not been prepared by the GL lifecycle");
    requireOwnerContext();
    return current;
}
void ngBindOpenGLDebugProvider(std::shared_ptr<nicxlive::core::RenderBackend> runtimeBackend) {
    if (!current) throw std::logic_error("GL debug provider has not been prepared by the GL lifecycle");
    requireOwnerContext();
    if (!std::dynamic_pointer_cast<runtime_gl::OpenGLRenderBackend>(runtimeBackend) ||
        runtimeBackend != nicxlive::core::getCurrentRenderBackend())
        throw std::logic_error("GL debug binding requires the selected concrete OpenGL runtime backend");
    boundRuntime = runtimeBackend;
    core::dbg::ngInstallDebugProvider(current, std::move(runtimeBackend));
}
void ngReleaseOpenGLDebugProvider() {
    if (current) {
        requireOwnerContext();
        current->release();
    }
    current.reset();
    ownerContext = nullptr;
    core::dbg::ngClearDebugProvider();
}
}
#undef NG_DEBUG_GL
