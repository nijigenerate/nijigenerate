#include "detail/psddepthmap_gpu.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include "detail/psddepthmap_shader_sources.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <nodes/common.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <bit>
#include <array>
#include <string>
#include <utility>
#define PSD_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)
#define glGenVertexArrays PSD_GL("glGenVertexArrays", PFNGLGENVERTEXARRAYSPROC)
#define glGenBuffers PSD_GL("glGenBuffers", PFNGLGENBUFFERSPROC)
#define glDeleteBuffers PSD_GL("glDeleteBuffers", PFNGLDELETEBUFFERSPROC)
#define glDeleteVertexArrays PSD_GL("glDeleteVertexArrays", PFNGLDELETEVERTEXARRAYSPROC)
#define glGenFramebuffers PSD_GL("glGenFramebuffers", PFNGLGENFRAMEBUFFERSPROC)
#define glDeleteFramebuffers PSD_GL("glDeleteFramebuffers", PFNGLDELETEFRAMEBUFFERSPROC)
#define glBindFramebuffer PSD_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)
#define glGenRenderbuffers PSD_GL("glGenRenderbuffers", PFNGLGENRENDERBUFFERSPROC)
#define glDeleteRenderbuffers PSD_GL("glDeleteRenderbuffers", PFNGLDELETERENDERBUFFERSPROC)
#define glBindRenderbuffer PSD_GL("glBindRenderbuffer", PFNGLBINDRENDERBUFFERPROC)
#define glRenderbufferStorage PSD_GL("glRenderbufferStorage", PFNGLRENDERBUFFERSTORAGEPROC)
#define glFramebufferTexture2D PSD_GL("glFramebufferTexture2D", PFNGLFRAMEBUFFERTEXTURE2DPROC)
#define glFramebufferRenderbuffer PSD_GL("glFramebufferRenderbuffer", PFNGLFRAMEBUFFERRENDERBUFFERPROC)
#define glCheckFramebufferStatus PSD_GL("glCheckFramebufferStatus", PFNGLCHECKFRAMEBUFFERSTATUSPROC)
#define glDrawBuffers PSD_GL("glDrawBuffers", PFNGLDRAWBUFFERSPROC)
#define glBlendEquationSeparate PSD_GL("glBlendEquationSeparate", PFNGLBLENDEQUATIONSEPARATEPROC)
#define glBlendFuncSeparate PSD_GL("glBlendFuncSeparate", PFNGLBLENDFUNCSEPARATEPROC)
#define glActiveTexture PSD_GL("glActiveTexture", PFNGLACTIVETEXTUREPROC)
#define glUseProgram PSD_GL("glUseProgram", PFNGLUSEPROGRAMPROC)
#define glBindVertexArray PSD_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)
#define glBindBuffer PSD_GL("glBindBuffer", PFNGLBINDBUFFERPROC)
#define glBufferData PSD_GL("glBufferData", PFNGLBUFFERDATAPROC)
#define glEnableVertexAttribArray PSD_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)
#define glDisableVertexAttribArray PSD_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)
#define glVertexAttribPointer PSD_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)

namespace nijigenerate::windows::psddepthmap::detail {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec4;
namespace { template<class F> struct Exit { F fn; ~Exit() noexcept(false) { fn(); } }; }
namespace {
void preparePsdDepthGpuClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<PsdDepth3DAdjustGpuRenderer>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<PsdDepth3DAdjustGpuRenderer>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(PsdDepth3DAdjustGpuRenderer), name);
        return true;
    }();
    (void)registered;
}
}
// Inherited object.Object defaults; Digital Mars, BSL-1.0.
compat::RetainedString PsdDepth3DAdjustGpuRenderer::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    preparePsdDepthGpuClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t PsdDepth3DAdjustGpuRenderer::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool PsdDepth3DAdjustGpuRenderer::opEquals(std::shared_ptr<PsdDepth3DAdjustGpuRenderer> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int PsdDepth3DAdjustGpuRenderer::opCmp(std::shared_ptr<PsdDepth3DAdjustGpuRenderer>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    preparePsdDepthGpuClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = helpers::allocatePsdDepthMemory([&] {
        std::string text = "need opCmp for class ";
        text += className.view();
        return compat::RetainedString(std::move(text));
    });
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
void PsdDepth3DAdjustGpuRenderer::ensureShader() {
    if (shader) return;
    shader = helpers::allocatePsdDepthObject<nicxlive::core::Shader>(
        compat::RetainedString::fromStaticLiteral(psdDepthAdjustVertexSource),
        compat::RetainedString::fromStaticLiteral(psdDepthAdjustFragmentSource));
    viewportSizeUniform = compat::ngRequireObject(shader)->getUniformLocation(
        compat::RetainedString::fromStaticLiteral("viewportSize"));
    centerUniform = compat::ngRequireObject(shader)->getUniformLocation(
        compat::RetainedString::fromStaticLiteral("center"));
    yawPitchUniform = compat::ngRequireObject(shader)->getUniformLocation(
        compat::RetainedString::fromStaticLiteral("yawPitch"));
    zoomPanUniform = compat::ngRequireObject(shader)->getUniformLocation(
        compat::RetainedString::fromStaticLiteral("zoomPan"));
    depthScaleUniform = compat::ngRequireObject(shader)->getUniformLocation(
        compat::RetainedString::fromStaticLiteral("depthDisplayScale"));
    layerDepthTransformUniform = compat::ngRequireObject(shader)->getUniformLocation(
        compat::RetainedString::fromStaticLiteral("layerDepthTransform"));
    textureUniform = compat::ngRequireObject(shader)->getUniformLocation(
        compat::RetainedString::fromStaticLiteral("tex"));
}
void PsdDepth3DAdjustGpuRenderer::ensureBuffers() {
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    if (ibo == 0) glGenBuffers(1, &ibo);
}
bool PsdDepth3DAdjustGpuRenderer::ensureTarget(int targetWidth, int targetHeight) {
    targetWidth = std::max(1, targetWidth); targetHeight = std::max(1, targetHeight);
    if (texture && (width != targetWidth || height != targetHeight)) {
        texture->dispose(); texture.reset();
        if (depthBuffer != 0) { glDeleteRenderbuffers(1, &depthBuffer); depthBuffer = 0; }
    }
    width = targetWidth; height = targetHeight;
    if (!texture) texture = nicxlive::core::Texture::createSource(width, height, 4, false, false);
    if (fbo == 0) glGenFramebuffers(1, &fbo);
    if (depthBuffer == 0) {
        glGenRenderbuffers(1, &depthBuffer); glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    return texture && fbo != 0 && depthBuffer != 0;
}
std::shared_ptr<nicxlive::core::Texture> PsdDepth3DAdjustGpuRenderer::renderScoped(
    int targetWidth, int targetHeight, const void* context, void (*invoke)(const void*)) {
    const auto receiver = weak_from_this().lock();
    GLint drawFbo = 0, readFbo = 0, renderbuffer = 0, program = 0, vertexArray = 0;
    GLint arrayBuffer = 0, elementArrayBuffer = 0, activeTexture = 0, activeTexture2D = 0, texture0 = 0;
    std::array<GLint, 4> viewport{};
    GLint depthFunc = 0, equationRgb = 0, equationAlpha = 0, srcRgb = 0, dstRgb = 0, srcAlpha = 0, dstAlpha = 0;
    std::array<GLfloat, 4> clearColor{NAN, NAN, NAN, NAN}; GLfloat clearDepth = NAN;
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST), cullEnabled = glIsEnabled(GL_CULL_FACE),
        blendEnabled = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer); glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray); glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer); glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTexture2D); glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0); glActiveTexture(static_cast<GLenum>(activeTexture));
    glGetIntegerv(GL_VIEWPORT, viewport.data()); glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRgb); glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor.data()); glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
    Exit restore{[&] {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFbo));
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glDepthFunc(static_cast<GLenum>(depthFunc));
        glBlendEquationSeparate(equationRgb, equationAlpha); glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]); glClearDepth(clearDepth);
        if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glUseProgram(program); glBindVertexArray(vertexArray); glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementArrayBuffer); glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture0); glActiveTexture(activeTexture); glBindTexture(GL_TEXTURE_2D, activeTexture2D);
    }};
    if (!ensureTarget(targetWidth, targetHeight)) return {};
    ensureShader(); ensureBuffers();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        compat::ngRequireObject(texture)->getTextureId(), 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    const GLenum attachment = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &attachment);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return {};
    glViewport(0, 0, width, height); glDisable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glEnable(GL_BLEND); nicxlive::core::nodes::inSetBlendMode(nicxlive::core::nodes::BlendMode::Normal);
    glClearColor(.08f, .09f, .10f, 1.f); glClearDepth(1.0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!invoke) compat::ngFailNullAccess(false);
    invoke(context);
    return texture;
}
bool PsdDepth3DAdjustGpuRenderer::capturePreview(int& resultWidth, int& resultHeight, compat::Slice<std::uint8_t>& rgba) {
    const auto receiver = weak_from_this().lock();
    resultWidth = 0; resultHeight = 0; rgba = {};
    if (fbo == 0 || !texture || width <= 0 || height <= 0) return false;
    GLint previousReadFbo = 0; glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFbo);
    Exit restore{[&] { glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFbo); }};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo); glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const auto rowBytes = static_cast<std::size_t>(width) * 4;
    compat::Slice<std::uint8_t> row; row.resize(rowBytes);
    const auto halfHeight = height / 2;
    for (int y = 0; y < halfHeight; ++y) {
        const auto opposite = height - y - 1;
        const auto top = static_cast<std::size_t>(y) * rowBytes, bottom = static_cast<std::size_t>(opposite) * rowBytes;
        {
            const auto source = compat::ngArraySlice(rgba, top, top + rowBytes);
            auto destination = row;
            std::copy(source.begin(), source.end(), destination.begin());
        }
        {
            const auto source = compat::ngArraySlice(rgba, bottom, bottom + rowBytes);
            auto destination = compat::ngArraySlice(rgba, top, top + rowBytes);
            std::copy(source.begin(), source.end(), destination.begin());
        }
        {
            const auto source = row;
            auto destination = compat::ngArraySlice(rgba, bottom, bottom + rowBytes);
            std::copy(source.begin(), source.end(), destination.begin());
        }
    }
    resultWidth = width; resultHeight = height; return true;
}
void PsdDepth3DAdjustGpuRenderer::drawLayer(std::shared_ptr<nicxlive::core::Texture> layerTexture,
    PsdDepth3DAdjustMesh& mesh, float centerX, float centerY, float yaw, float pitch, float zoom,
    Vec2 pan, float depthDisplayScale, PsdDepthLayerEdit bakedTransform, PsdDepthLayerEdit currentTransform) {
    const auto receiver = weak_from_this().lock();
    if (!layerTexture || mesh.vertexData.empty() || mesh.indices.empty()) return;
    compat::ngRequireObject(shader)->use();
    compat::ngRequireObject(shader)->setUniform(viewportSizeUniform, Vec2{float(width), float(height)});
    compat::ngRequireObject(shader)->setUniform(centerUniform, Vec2{centerX, centerY});
    compat::ngRequireObject(shader)->setUniform(yawPitchUniform,
        Vec4{std::cos(yaw), std::sin(yaw), std::cos(pitch), std::sin(pitch)});
    compat::ngRequireObject(shader)->setUniform(zoomPanUniform, Vec4{zoom, pan.x, pan.y, 0.f});
    compat::ngRequireObject(shader)->setUniform(depthScaleUniform, depthDisplayScale);
    compat::ngRequireObject(shader)->setUniform(layerDepthTransformUniform,
        Vec4{bakedTransform.zScale, bakedTransform.zOffset, currentTransform.zScale, currentTransform.zOffset});
    compat::ngRequireObject(shader)->setUniform(textureUniform, 0);
    layerTexture->bind(0);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexData.size() * sizeof(float), mesh.vertexData.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(std::uint32_t), mesh.indices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(float) * 5, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(float) * 5, reinterpret_cast<void*>(sizeof(float) * 3));
    glDrawElements(GL_TRIANGLES, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(mesh.indices.size())), GL_UNSIGNED_INT, nullptr);
    glDisableVertexAttribArray(0); glDisableVertexAttribArray(1);
}
void PsdDepth3DAdjustGpuRenderer::dispose() {
    const auto receiver = weak_from_this().lock();
    if (texture) { texture->dispose(); texture.reset(); }
    if (depthBuffer != 0) { glDeleteRenderbuffers(1, &depthBuffer); depthBuffer = 0; }
    if (fbo != 0) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (vbo != 0) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ibo != 0) { glDeleteBuffers(1, &ibo); ibo = 0; }
    if (vao != 0) { glDeleteVertexArrays(1, &vao); vao = 0; }
}
}
