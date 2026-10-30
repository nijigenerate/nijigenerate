// Port of independent nijilive OpenGL runtime.d, BSD-2-Clause.
// Copyright (c) 2020 Inochi2D Project; (c) 2024 nijigenerate Project.
// Full BSD notice: native/port/licenses/nijilive-difference-aggregation.txt.
// Orthographic specialization: inmath 1.0.6, David Herberth / Inochi2D Project.
// Full MIT notice: native/port/licenses/inmath-scene-orthographic.txt.
#include "runtime_gl_runtime.hpp"
#include "runtime_gl_scene_shader_assets.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <runtime_state.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <initializer_list>
#include <utility>

#if defined(_WIN32)
extern "C" {
__declspec(dllexport) int NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

// Resolve every GL call through the owning current SDL context.
#define glActiveTexture ::nijigenerate::platform::ngGLBackendProcedure<PFNGLACTIVETEXTUREPROC>("glActiveTexture")
#define glBindBuffer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer")
#define glBindFramebuffer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer")
#define glBindTexture ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glBindTexture)>("glBindTexture")
#define glBindVertexArray ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray")
#define glBlendEquation ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBLENDEQUATIONPROC>("glBlendEquation")
#define glBlendFunc ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glBlendFunc)>("glBlendFunc")
#define glBlitFramebuffer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBLITFRAMEBUFFERPROC>("glBlitFramebuffer")
#define glBufferData ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBUFFERDATAPROC>("glBufferData")
#define glClear ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glClear)>("glClear")
#define glClearColor ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glClearColor)>("glClearColor")
#define glDisable ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDisable)>("glDisable")
#define glDisableVertexAttribArray ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDISABLEVERTEXATTRIBARRAYPROC>("glDisableVertexAttribArray")
#define glDisablei ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDISABLEIPROC>("glDisablei")
#define glDrawArrays ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDrawArrays)>("glDrawArrays")
#define glDrawBuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDRAWBUFFERSPROC>("glDrawBuffers")
#define glEnable ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glEnable)>("glEnable")
#define glEnableVertexAttribArray ::nijigenerate::platform::ngGLBackendProcedure<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray")
#define glEnablei ::nijigenerate::platform::ngGLBackendProcedure<PFNGLENABLEIPROC>("glEnablei")
#define glFlush ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glFlush)>("glFlush")
#define glFramebufferTexture2D ::nijigenerate::platform::ngGLBackendProcedure<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D")
#define glGenBuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENBUFFERSPROC>("glGenBuffers")
#define glGenFramebuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers")
#define glGenTextures ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGenTextures)>("glGenTextures")
#define glGenVertexArrays ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays")
#define glGenerateMipmap ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENERATEMIPMAPPROC>("glGenerateMipmap")
#define glGetIntegerv ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGetIntegerv)>("glGetIntegerv")
#define glGetTexImage ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGetTexImage)>("glGetTexImage")
#define glTexImage2D ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glTexImage2D)>("glTexImage2D")
#define glTexParameteri ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glTexParameteri)>("glTexParameteri")
#define glVertexAttribPointer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer")
#define glViewport ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glViewport)>("glViewport")

namespace nijigenerate::backend::runtime_gl {
void oglResetBoundAlbedo();
using namespace nicxlive::core::math;
using nicxlive::core::inGetViewport;
using nicxlive::core::inViewportWidth;
using nicxlive::core::inViewportHeight;
using nicxlive::core::Shader;
using nicxlive::core::shaderAsset;
namespace {
thread_local GLuint sceneVAO = 0, sceneVBO = 0;
struct Targets { GLuint buffer = 0, albedo = 0, emissive = 0, bump = 0, stencil = 0; };
thread_local Targets mainTargets, compositeTargets, blendTargets;
thread_local PostProcessingShader basicSceneShader, basicSceneLighting;
thread_local nicxlive::core::common::SharedArray<PostProcessingShader> postProcessingStack;
thread_local bool isCompositing = false;
struct CompositeFrameState { GLint framebuffer{}; GLint viewport[4]{}; };
thread_local nicxlive::core::common::SharedArray<CompositeFrameState> compositeScopeStack;
}
PostProcessingShader::PostProcessingShader(std::shared_ptr<Shader> value) : shader(value) {
    using String = nicxlive::core::d_runtime::RetainedString;
    compat::ngRequireObject(value)->use();
    const auto albedoReceiver = value;
    const auto albedo = compat::ngRequireObject(value)->getUniformLocation(String::fromStaticLiteral("albedo"));
    compat::ngRequireObject(albedoReceiver)->setUniform(albedo, 0);
    const auto emissiveReceiver = value;
    const auto emissive = compat::ngRequireObject(value)->getUniformLocation(String::fromStaticLiteral("emissive"));
    compat::ngRequireObject(emissiveReceiver)->setUniform(emissive, 1);
    const auto bumpReceiver = value;
    const auto bump = compat::ngRequireObject(value)->getUniformLocation(String::fromStaticLiteral("bumpmap"));
    compat::ngRequireObject(bumpReceiver)->setUniform(bump, 2);
}
unsigned PostProcessingShader::getUniform(String name) {
    if (hasUniform(name)) {
        const auto found = uniformCache.find(name);
        if (found == uniformCache.end()) throw compat::ngStaticError<compat::RangeError>(
            String::fromStaticLiteral("nijilive/core/render/backends/opengl/runtime.d"), 46);
        return static_cast<unsigned>(found->second);
    }
    const auto location = compat::ngRequireObject(shader)->getUniformLocation(name);
    uniformCache[name] = location;
    return static_cast<unsigned>(location);
}
bool PostProcessingShader::hasUniform(String name) const {
    return uniformCache.contains(name);
}
namespace {
int viewportLast(nicxlive::core::common::SharedArray<int>& values, std::size_t line) {
    const auto index = values.size() - 1;
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        nicxlive::core::d_runtime::RetainedString::fromStaticLiteral(
            "nijilive/core/render/backends/opengl/runtime.d"), line);
    return values.data()[index];
}
void attach(const Targets& t) {
    glBindFramebuffer(GL_FRAMEBUFFER, t.buffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.albedo, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, t.emissive, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, t.bump, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, t.stencil, 0);
}
void drawBuffers(std::initializer_list<GLenum> values) {
    glDrawBuffers(static_cast<GLsizei>(values.size()), values.begin());
}
nicxlive::core::common::SharedArray<float> uploadQuad(Vec4 area) {
    nicxlive::core::common::SharedArray<float> data{area.x, area.y + area.w, 0, 0, area.x, area.y, 0, 1,
        area.x + area.z, area.y + area.w, 1, 0, area.x + area.z, area.y + area.w, 1, 0,
        area.x, area.y, 0, 1, area.x + area.z, area.y, 1, 1};
    glBindBuffer(GL_ARRAY_BUFFER, sceneVBO);
    glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    return data;
}
// Preserve inmath's orthographic contract, including zero-extent assertions.
Mat4 sceneProjection(Vec4 area) {
    const float farPlane = std::max(area.z, area.w);
#ifndef NDEBUG
    using String = nicxlive::core::d_runtime::RetainedString;
    const auto file = String::fromStaticLiteral("inmath/linalg.d");
    if (!(area.z - 0.0f != 0)) throw compat::ngStaticError<compat::AssertError>(String{}, file, 1395);
    if (!(0.0f - area.w != 0)) throw compat::ngStaticError<compat::AssertError>(String{}, file, 1396);
    if (!(farPlane - 0.0f != 0)) throw compat::ngStaticError<compat::AssertError>(String{}, file, 1397);
#endif
    Mat4 result{};
    result[0][0] = 2 / (area.z - 0.0f);
    result[0][3] = -(area.z + 0.0f) / (area.z - 0.0f);
    result[1][1] = 2 / (0.0f - area.w);
    result[1][3] = -(0.0f + area.w) / (0.0f - area.w);
    result[2][2] = -2 / (farPlane - 0.0f);
    result[2][3] = -(farPlane + 0.0f) / (farPlane - 0.0f);
    result[3][3] = 1;
    return result;
}
void renderScene(Vec4 area, PostProcessingShader value, GLuint albedo, GLuint emissive, GLuint bump) {
    glViewport(0, 0, compat::ngFloatToInt32(area.z), compat::ngFloatToInt32(area.w));
    glBindVertexArray(sceneVAO);
    glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    using String = nicxlive::core::d_runtime::RetainedString;
    compat::ngRequireObject(value.shader)->use();
    const auto mvpReceiver = compat::ngRequireObject(value.shader);
    const auto mvpUniform = static_cast<int>(value.getUniform(String::fromStaticLiteral("mvp")));
    const auto projection = sceneProjection(area);
    const auto translation = Mat4::translation(area.x, area.y, 0);
    compat::ngRequireObject(mvpReceiver)->setUniform(mvpUniform, projection * translation);
    const GLint ambient = value.getUniform(String::fromStaticLiteral("ambientLight"));
    if (ambient != -1) compat::ngRequireObject(value.shader)->setUniform(ambient,
        Vec3{inSceneAmbientLight[0], inSceneAmbientLight[1], inSceneAmbientLight[2]});
    const GLint fbSize = value.getUniform(String::fromStaticLiteral("fbSize"));
    if (fbSize != -1) {
        const auto receiver = compat::ngRequireObject(value.shader);
        const auto width = viewportLast(inViewportWidth, 120);
        const auto height = viewportLast(inViewportHeight, 120);
        compat::ngRequireObject(receiver)->setUniform(fbSize,
            Vec2{static_cast<float>(width), static_cast<float>(height)});
    }
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, albedo);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, emissive);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, bump);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0); glDisableVertexAttribArray(1); glDisable(GL_BLEND);
}
}
void oglInitRenderer() {
    nicxlive::core::inSetViewport(640, 480);
#ifndef NICXLIVE_SOURCE_NO_RENDER
    nijigenerate::core::dbg::inInitDebug();
    basicSceneShader = PostProcessingShader(std::make_shared<Shader>(
        shaderAsset<scene_assets::sceneVertex, scene_assets::sceneFragment>()));
    glGenVertexArrays(1, &sceneVAO); glGenBuffers(1, &sceneVBO);
    glGenFramebuffers(1, &mainTargets.buffer);
    glGenFramebuffers(1, &compositeTargets.buffer);
    glGenFramebuffers(1, &blendTargets.buffer);
    for (auto* t : {&mainTargets, &compositeTargets, &blendTargets}) {
        glGenTextures(1, &t->albedo); glGenTextures(1, &t->emissive);
        glGenTextures(1, &t->bump); glGenTextures(1, &t->stencil);
    }
    attach(mainTargets); attach(compositeTargets); attach(blendTargets);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}
void oglBeginScene() {
    oglResetBoundAlbedo();
    glBindVertexArray(sceneVAO); glEnable(GL_BLEND);
    glEnablei(GL_BLEND, 0); glEnablei(GL_BLEND, 1); glEnablei(GL_BLEND, 2);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    const auto width = viewportLast(inViewportWidth, 230);
    const auto height = viewportLast(inViewportHeight, 230);
    glViewport(0, 0, width, height);
    attach(mainTargets); attach(compositeTargets);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, compositeTargets.buffer);
    drawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2});
    glClearColor(0, 0, 0, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mainTargets.buffer);
    drawBuffers({GL_COLOR_ATTACHMENT0});
    float r, g, b, a; nicxlive::core::inGetClearColor(r, g, b, a);
    glClearColor(r, g, b, a); glClear(GL_COLOR_BUFFER_BIT);
    drawBuffers({GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2});
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2});
}
void oglReleaseRenderer() {
    // C++ shared ownership finalizes shaders deterministically while GL is current.
    postProcessingStack.clear();
    basicSceneLighting = {};
    basicSceneShader = {};
    const auto deleteTextures = platform::ngGLBackendProcedure<decltype(&::glDeleteTextures)>("glDeleteTextures");
    const auto deleteFramebuffers = platform::ngGLBackendProcedure<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
    for (auto* target : {&mainTargets, &compositeTargets, &blendTargets}) {
        const GLuint textures[]{target->albedo, target->emissive, target->bump, target->stencil};
        deleteTextures(4, textures);
        if (target->buffer) deleteFramebuffers(1, &target->buffer);
        *target = {};
    }
    if (sceneVBO) platform::ngGLBackendProcedure<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers")(1, &sceneVBO);
    if (sceneVAO) platform::ngGLBackendProcedure<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays")(1, &sceneVAO);
    sceneVBO = 0; sceneVAO = 0;
    compositeScopeStack.clear();
    isCompositing = false;
}
void oglBeginComposite() {}
void oglEndComposite() {}
void oglEndScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisablei(GL_BLEND, 0); glDisablei(GL_BLEND, 1); glDisablei(GL_BLEND, 2);
    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glDisable(GL_BLEND); glFlush();
    drawBuffers({GL_COLOR_ATTACHMENT0});
}
void oglPostProcessScene() {
    if (postProcessingStack.empty()) return;
    bool targetBuffer = false;
    float r, g, b, a; nicxlive::core::inGetClearColor(r, g, b, a);
    const auto width = viewportLast(inViewportWidth, 316);
    const auto height = viewportLast(inViewportHeight, 316);
    const Vec4 area{0, 0, static_cast<float>(width), static_cast<float>(height)};
    [[maybe_unused]] const auto quadData = uploadQuad(area);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, mainTargets.emissive);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindFramebuffer(GL_FRAMEBUFFER, compositeTargets.buffer);
    drawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2});
    glClearColor(r, g, b, a); glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, mainTargets.buffer);
    drawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2});
    const auto capturedStack = postProcessingStack;
    for (auto value : capturedStack) {
        targetBuffer = !targetBuffer;
        const auto& src = targetBuffer ? mainTargets : compositeTargets;
        glBindFramebuffer(GL_FRAMEBUFFER, targetBuffer ? compositeTargets.buffer : mainTargets.buffer);
        renderScene(area, value, src.albedo, src.emissive, src.bump);
    }
    if (targetBuffer) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, compositeTargets.buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mainTargets.buffer);
        const auto sourceWidth = viewportLast(inViewportWidth, 366);
        const auto sourceHeight = viewportLast(inViewportHeight, 366);
        const auto targetWidth = viewportLast(inViewportWidth, 367);
        const auto targetHeight = viewportLast(inViewportHeight, 367);
        glBlitFramebuffer(0, 0, sourceWidth, sourceHeight, 0, 0, targetWidth, targetHeight,
            GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void oglAddBasicLightingPostProcess() {
    postProcessingStack.push_back(PostProcessingShader(std::make_shared<Shader>(
        shaderAsset<scene_assets::sceneVertex, scene_assets::lightingFragment>())));
}
nicxlive::core::common::SharedArray<PostProcessingShader>& oglGetPostProcessingStack() { return postProcessingStack; }
void oglDrawScene(Vec4 area) {
    [[maybe_unused]] const auto quadData = uploadQuad(area);
    renderScene(area, basicSceneShader, mainTargets.albedo, mainTargets.emissive, mainTargets.bump);
}
void oglPrepareCompositeScene() {
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, compositeTargets.albedo);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, compositeTargets.emissive);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, compositeTargets.bump);
}
unsigned oglGetFramebuffer() { return mainTargets.buffer; }
unsigned oglGetRenderImage() { return mainTargets.albedo; }
unsigned oglGetCompositeImage() { return compositeTargets.albedo; }
unsigned oglGetCompositeFramebuffer() { return compositeTargets.buffer; }
unsigned oglGetBlendFramebuffer() { return blendTargets.buffer; }
unsigned oglGetMainAlbedo() { return mainTargets.albedo; }
unsigned oglGetMainEmissive() { return mainTargets.emissive; }
unsigned oglGetMainBump() { return mainTargets.bump; }
unsigned oglGetCompositeEmissive() { return compositeTargets.emissive; }
unsigned oglGetCompositeBump() { return compositeTargets.bump; }
unsigned oglGetBlendAlbedo() { return blendTargets.albedo; }
unsigned oglGetBlendEmissive() { return blendTargets.emissive; }
unsigned oglGetBlendBump() { return blendTargets.bump; }
void oglRebindActiveTargets() {
    GLint previous; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    attach(mainTargets); attach(compositeTargets);
    glBindFramebuffer(GL_FRAMEBUFFER, previous);
}
void oglSwapMainCompositeBuffers() {}
void oglResizeViewport(int width, int height) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    for (const auto* t : {&mainTargets, &compositeTargets, &blendTargets}) {
        for (std::size_t index = 0; index < 3; ++index) {
            const auto texture = index == 0 ? t->albedo : index == 1 ? t->emissive : t->bump;
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                index == 1 ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glBindTexture(GL_TEXTURE_2D, t->stencil);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0,
            GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        attach(*t);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, width, height);
#else
    (void)width; (void)height;
#endif
}
void oglDumpViewport(std::vector<std::uint8_t>& destination, int width, int height) {
    oglDumpViewport(std::span<std::uint8_t>(destination), width, height);
}
void oglDumpViewport(std::span<std::uint8_t> destination, int width, int height) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    if (width == 0 || height == 0) return;
    glBindTexture(GL_TEXTURE_2D, mainTargets.albedo);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, destination.data());
#else
    (void)destination; (void)width; (void)height;
#endif
}
void oglDumpViewport(nicxlive::core::common::SharedArray<std::uint8_t>& destination, int width, int height) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    if (width == 0 || height == 0) return;
    glBindTexture(GL_TEXTURE_2D, mainTargets.albedo);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, destination.data());
#else
    (void)destination; (void)width; (void)height;
#endif
}
}

#undef glActiveTexture
#undef glBindBuffer
#undef glBindFramebuffer
#undef glBindTexture
#undef glBindVertexArray
#undef glBlendEquation
#undef glBlendFunc
#undef glBlitFramebuffer
#undef glBufferData
#undef glClear
#undef glClearColor
#undef glDisable
#undef glDisableVertexAttribArray
#undef glDisablei
#undef glDrawArrays
#undef glDrawBuffers
#undef glEnable
#undef glEnableVertexAttribArray
#undef glEnablei
#undef glFlush
#undef glFramebufferTexture2D
#undef glGenBuffers
#undef glGenFramebuffers
#undef glGenTextures
#undef glGenVertexArrays
#undef glGenerateMipmap
#undef glGetIntegerv
#undef glGetTexImage
#undef glTexImage2D
#undef glTexParameteri
#undef glVertexAttribPointer
#undef glViewport
