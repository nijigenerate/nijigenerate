// Port of the independent opengl/part.d production renderer.
#include "runtime_gl_part.hpp"
#include "runtime_gl_blend.hpp"
#include "runtime_gl_runtime.hpp"
#include "runtime_gl_drawable_buffers.hpp"
#include "runtime_gl_buffer_sync.hpp"
#include "runtime_gl_geometry_shaders.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <runtime_state.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include "runtime_gl_geometry_calls.hpp"
namespace nijigenerate::backend::runtime_gl {
using nicxlive::core::nodes::PartDrawPacket;
using nicxlive::core::d_runtime::RetainedString;
std::shared_ptr<nicxlive::core::Shader> partShader;
std::shared_ptr<nicxlive::core::Shader> partShaderStage1, partShaderStage2, partMaskShader;
int gopacity{}, gMultColor{}, gScreenColor{}, gEmissionStrength{}, mvp{}, offset{};
int gs1mvp{}, gs1offset{}, gs1opacity{}, gs1MultColor{}, gs1ScreenColor{};
int gs2mvp{}, gs2offset{}, gs2opacity{}, gs2MultColor{}, gs2ScreenColor{}, gs2EmissionStrength{};
int mmvp{}, mthreshold{};
bool partBackendInitialized = false;
namespace {
bool boundValid{};
std::uint32_t boundAlbedo{}, boundBackend{};
constexpr GLenum attachments[]{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
template<std::size_t N>
void sampler(const std::shared_ptr<nicxlive::core::Shader>& shader, const char (&name)[N], int unit) {
    const auto receiver = compat::ngRequireObject(shader);
    const auto location = compat::ngRequireObject(shader)->getUniformLocation(RetainedString::fromStaticLiteral(name));
    receiver->setUniform(location, unit);
}
void setupStage(const PartDrawPacket& packet, int stage) {
    const auto matrix = packet.renderMatrix * packet.modelMatrix;
    glDrawBuffers(stage == 0 ? 1 : stage == 1 ? 2 : 3, attachments + (stage == 1 ? 1 : 0));
    const auto currentShader = [stage] {
        return compat::ngRequireObject(stage == 0 ? partShaderStage1 : stage == 1 ? partShaderStage2 : partShader);
    };
    currentShader()->use();
    currentShader()->setUniform(stage == 0 ? gs1offset : stage == 1 ? gs2offset : offset, packet.origin);
    currentShader()->setUniform(stage == 0 ? gs1mvp : stage == 1 ? gs2mvp : mvp, matrix);
    currentShader()->setUniform(stage == 0 ? gs1opacity : stage == 1 ? gs2opacity : gopacity, packet.opacity);
    if (stage != 0) currentShader()->setUniform(stage == 1 ? gs2EmissionStrength : gEmissionStrength, packet.emissionStrength);
    currentShader()->setUniform(stage == 0 ? gs1MultColor : stage == 1 ? gs2MultColor : gMultColor, packet.clampedTint);
    currentShader()->setUniform(stage == 0 ? gs1ScreenColor : stage == 1 ? gs2ScreenColor : gScreenColor, packet.clampedScreen);
    oglSetBlendMode(packet.blendMode, stage != 0);
}
void attribute(unsigned location, unsigned buffer, std::size_t offsetFloats) {
    glEnableVertexAttribArray(location); glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(location, 1, GL_FLOAT, GL_FALSE, 0,
        reinterpret_cast<const void*>(offsetFloats * sizeof(float)));
}
void renderStage(const PartDrawPacket& packet, bool advanced) {
    if (!packet.indexBuffer || !packet.indexCount || !packet.vertexCount ||
        !packet.vertexAtlasStride || !packet.uvAtlasStride || !packet.deformAtlasStride) return;
    const auto vertex = oglGetSharedVertexBuffer(), uv = oglGetSharedUvBuffer(), deform = oglGetSharedDeformBuffer();
    if (!vertex || !uv || !deform) return;
    attribute(0, vertex, packet.vertexOffset);
    attribute(1, vertex, std::size_t(packet.vertexOffset) + packet.vertexAtlasStride);
    attribute(2, uv, packet.uvOffset);
    attribute(3, uv, std::size_t(packet.uvOffset) + packet.uvAtlasStride);
    attribute(4, deform, packet.deformOffset);
    attribute(5, deform, std::size_t(packet.deformOffset) + packet.deformAtlasStride);
    oglDrawDrawableElements(packet.indexBuffer, packet.indexCount);
    markBufferInUse(vertex); markBufferInUse(uv); markBufferInUse(deform);
    for (unsigned i = 0; i < 6; ++i) glDisableVertexAttribArray(i);
    if (advanced) oglBlendModeBarrier(packet.blendMode);
}
struct SavedState {
    GLint drawFbo{}, readFbo{}, readBuffer{}, viewport[4]{}, drawBuffers[4]{}, drawCount{};
    GLfloat clearColor[4]{};
    GLint srcRGB{}, dstRGB{}, srcA{}, dstA{}, equationRGB{}, equationA{};
    GLboolean colorMask[4]{}, depth{}, stencil{};
    SavedState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
        glGetIntegerv(GL_READ_BUFFER, &readBuffer); glGetIntegerv(GL_VIEWPORT, viewport);
        GLint count{}; glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count); count = std::min(count, 4);
        for (int i = 0; i < count; ++i) {
            glGetIntegerv(GL_DRAW_BUFFER0 + i, &drawBuffers[i]);
            if (drawBuffers[i] != GL_NONE) drawCount = i + 1;
        }
        if (!drawCount) {
            drawCount = 3;
            for (int i = 0; i < 3; ++i) drawBuffers[i] = attachments[i];
        }
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB); glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcA); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstA);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRGB); glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationA);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        depth = glIsEnabled(GL_DEPTH_TEST); stencil = glIsEnabled(GL_STENCIL_TEST);
    }
    void restore() const {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo); glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        glReadBuffer(readBuffer);
        GLenum buffers[4]{};
        for (int i = 0; i < 4; ++i) buffers[i] = static_cast<GLenum>(drawBuffers[i]);
        glDrawBuffers(drawCount, buffers);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glBlendEquationSeparate(equationRGB, equationA); glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        glDrawBuffers(3, attachments);
    }
};
void copyAttachments(unsigned from, unsigned to, int width, int height) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, from); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, to);
    for (int i = 0; i < 3; ++i) {
        glReadBuffer(attachments[i]); glDrawBuffer(attachments[i]);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}
}
void oglInitPartBackendResources() {
    if (partBackendInitialized) return;
    partBackendInitialized = true;
    const auto createShader = [](std::string_view fragment) {
        return std::make_shared<nicxlive::core::Shader>(
            nicxlive::core::d_runtime::RetainedString::fromStaticView(geometry_shaders::basicVertex),
            nicxlive::core::d_runtime::RetainedString::fromStaticView(fragment));
    };
    partShader = createShader(geometry_shaders::basicFragment);
    partShaderStage1 = createShader(geometry_shaders::stage1);
    partShaderStage2 = createShader(geometry_shaders::stage2);
    partMaskShader = createShader(geometry_shaders::partMask);
    oglBindDrawableVao();
    compat::ngRequireObject(partShader)->use();
    sampler(partShader, "albedo", 0); sampler(partShader, "emissive", 1); sampler(partShader, "bumpmap", 2);
    mvp = compat::ngRequireObject(partShader)->getUniformLocation(RetainedString::fromStaticLiteral("mvp"));
    offset = compat::ngRequireObject(partShader)->getUniformLocation(RetainedString::fromStaticLiteral("offset"));
    gopacity = compat::ngRequireObject(partShader)->getUniformLocation(RetainedString::fromStaticLiteral("opacity"));
    gMultColor = compat::ngRequireObject(partShader)->getUniformLocation(RetainedString::fromStaticLiteral("multColor"));
    gScreenColor = compat::ngRequireObject(partShader)->getUniformLocation(RetainedString::fromStaticLiteral("screenColor"));
    gEmissionStrength = compat::ngRequireObject(partShader)->getUniformLocation(RetainedString::fromStaticLiteral("emissionStrength"));
    compat::ngRequireObject(partShaderStage1)->use();
    // The independent source intentionally queries partShader for this sampler.
    {
        const auto stage1Sampler = compat::ngRequireObject(partShaderStage1);
        const auto stage1Albedo = compat::ngRequireObject(partShader)->getUniformLocation(
            RetainedString::fromStaticLiteral("albedo"));
        stage1Sampler->setUniform(stage1Albedo, 0);
    }
    gs1mvp = compat::ngRequireObject(partShaderStage1)->getUniformLocation(RetainedString::fromStaticLiteral("mvp"));
    gs1offset = compat::ngRequireObject(partShaderStage1)->getUniformLocation(RetainedString::fromStaticLiteral("offset"));
    gs1opacity = compat::ngRequireObject(partShaderStage1)->getUniformLocation(RetainedString::fromStaticLiteral("opacity"));
    gs1MultColor = compat::ngRequireObject(partShaderStage1)->getUniformLocation(RetainedString::fromStaticLiteral("multColor"));
    gs1ScreenColor = compat::ngRequireObject(partShaderStage1)->getUniformLocation(RetainedString::fromStaticLiteral("screenColor"));
    compat::ngRequireObject(partShaderStage2)->use();
    sampler(partShaderStage2, "emissive", 1); sampler(partShaderStage2, "bumpmap", 2);
    gs2mvp = compat::ngRequireObject(partShaderStage2)->getUniformLocation(RetainedString::fromStaticLiteral("mvp"));
    gs2offset = compat::ngRequireObject(partShaderStage2)->getUniformLocation(RetainedString::fromStaticLiteral("offset"));
    gs2opacity = compat::ngRequireObject(partShaderStage2)->getUniformLocation(RetainedString::fromStaticLiteral("opacity"));
    gs2MultColor = compat::ngRequireObject(partShaderStage2)->getUniformLocation(RetainedString::fromStaticLiteral("multColor"));
    gs2ScreenColor = compat::ngRequireObject(partShaderStage2)->getUniformLocation(RetainedString::fromStaticLiteral("screenColor"));
    gs2EmissionStrength = compat::ngRequireObject(partShaderStage2)->getUniformLocation(RetainedString::fromStaticLiteral("emissionStrength"));
    compat::ngRequireObject(partMaskShader)->use();
    sampler(partMaskShader, "albedo", 0); sampler(partMaskShader, "emissive", 1); sampler(partMaskShader, "bumpmap", 2);
    mmvp = compat::ngRequireObject(partMaskShader)->getUniformLocation(RetainedString::fromStaticLiteral("mvp"));
    mthreshold = compat::ngRequireObject(partMaskShader)->getUniformLocation(RetainedString::fromStaticLiteral("threshold"));
}
void oglResetBoundAlbedo() { boundValid = false; boundAlbedo = boundBackend = 0; }
void oglDrawPartPacket(const PartDrawPacket& packet) { if (packet.renderable) oglExecutePartPacket(packet); }
void oglExecutePartPacket(const PartDrawPacket& packet) {
    // Native packets snapshot texture identities and backend IDs instead of retaining D Texture references.
    if (!packet.textureCount) return;
    oglBindDrawableVao();
    if (!boundValid || boundAlbedo != packet.textureUUIDs[0] || boundBackend != packet.textureBackendIds[0]) {
        for (unsigned i = 0; i < std::min(packet.textureCount, 3u); ++i) {
            glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, packet.textureBackendIds[i]);
        }
        boundValid = true; boundAlbedo = packet.textureUUIDs[0]; boundBackend = packet.textureBackendIds[0];
    }
    if (packet.isMask) {
        compat::ngRequireObject(partMaskShader)->use();
        // Preserve the source's shared offset location in this mask branch.
        compat::ngRequireObject(partMaskShader)->setUniform(offset, packet.origin);
        compat::ngRequireObject(partMaskShader)->setUniform(mmvp, packet.renderMatrix * packet.modelMatrix);
        compat::ngRequireObject(partMaskShader)->setUniform(mthreshold, packet.maskThreshold);
        glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        renderStage(packet, false);
    } else if (oglUseMultistageBlending(packet.blendMode)) {
        setupStage(packet, 0); renderStage(packet, true);
        if (packet.hasEmissionOrBumpmap) { setupStage(packet, 1); renderStage(packet, false); }
    } else {
        if (oglIsTripleBufferFallbackEnabled() && oglGetBlendShader(packet.blendMode)) {
            SavedState previous;
            if (static_cast<unsigned>(previous.drawFbo) != oglGetFramebuffer() &&
                static_cast<unsigned>(previous.drawFbo) != oglGetCompositeFramebuffer()) {
                setupStage(packet, 2); renderStage(packet, false); previous.restore(); return;
            }
            int width{}, height{}; nicxlive::core::inGetViewport(width, height);
            const auto target = oglGetBlendFramebuffer();
            copyAttachments(previous.drawFbo, target, width, height);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target); glDrawBuffers(3, attachments);
            glViewport(0, 0, width, height); setupStage(packet, 2); renderStage(packet, false);
            copyAttachments(target, previous.drawFbo, width, height);
            previous.restore(); oglResetBoundAlbedo(); return;
        }
        setupStage(packet, 2); renderStage(packet, false);
    }
    glDrawBuffers(3, attachments); glBlendEquation(GL_FUNC_ADD);
}
}
