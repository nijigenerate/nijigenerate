// Copyright 2020 Inochi2D Project, 2024 nijigenerate Project. Author: Luna Nielsen.
// Complete independent opengl/blend.d selected OpenGL port, BSD-2-Clause.
#include "runtime_gl_blend.hpp"
#include "runtime_gl_blend_shaders.hpp"
#include <common/resource_key_map.hpp>
#include <nodes/common.hpp>
#include <SDL3/SDL_video.h>
#include <array>
#include <cstdint>
#include <functional>
#include "runtime_gl_geometry_calls.hpp"

namespace nijigenerate::backend::runtime_gl {
using nicxlive::core::Shader;
using nicxlive::core::d_runtime::RetainedString;

#ifndef NICXLIVE_SOURCE_NO_RENDER
namespace {
// The original __gshared AA is shared by this rendering context, not thread-local.
struct BlendKeyHash {
    std::size_t operator()(BlendMode mode) const noexcept {
        return static_cast<std::uint32_t>(mode);
    }
};
nicxlive::core::common::ResourceKeyMap<BlendMode, std::shared_ptr<Shader>,
    BlendKeyHash, std::equal_to<BlendMode>> blendShaders;

void ensureBlendShadersInitialized() {
    if (!blendShaders.empty()) return;
    auto advancedBlendShader = std::make_shared<Shader>(nicxlive::core::shaderAsset<
        blend_shaders::basicVertexSource, blend_shaders::advancedBlendSource>());
    constexpr BlendMode advancedModes[]{
        BlendMode::Multiply, BlendMode::Screen, BlendMode::Overlay,
        BlendMode::Darken, BlendMode::Lighten, BlendMode::ColorDodge,
        BlendMode::ColorBurn, BlendMode::HardLight, BlendMode::SoftLight,
        BlendMode::Difference, BlendMode::Exclusion
    };
    // Keep the Source map's partial initialization and observable nonempty guard.
    for (const auto mode : advancedModes) blendShaders[mode] = advancedBlendShader;
}
}

std::shared_ptr<Shader> oglGetBlendShader(BlendMode mode) {
    ensureBlendShadersInitialized();
    const auto found = blendShaders.findRetained(mode);
    return found ? *found : nullptr;
}

void oglBlendToBuffer(std::shared_ptr<Shader> shader, BlendMode mode, unsigned target,
    unsigned bgAlbedo, unsigned bgEmissive, unsigned bgBump,
    unsigned fgAlbedo, unsigned fgEmissive, unsigned fgBump) {
    if (!shader) return;

    glBindFramebuffer(GL_FRAMEBUFFER, target);
    const GLenum attachments[]{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    shader->use();
    const int modeUniform = shader->getUniformLocation(RetainedString::fromStaticLiteral("blend_mode"));
    if (modeUniform != -1) shader->setUniform(modeUniform, static_cast<int>(mode));

    // All six texture bindings precede every sampler query/write in the original.
    const unsigned textures[]{bgAlbedo, bgEmissive, bgBump, fgAlbedo, fgEmissive, fgBump};
    for (unsigned i = 0; i < 6; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    shader->setUniform(shader->getUniformLocation(RetainedString::fromStaticLiteral("bg_albedo")), 0);
    shader->setUniform(shader->getUniformLocation(RetainedString::fromStaticLiteral("bg_emissive")), 1);
    shader->setUniform(shader->getUniformLocation(RetainedString::fromStaticLiteral("bg_bump")), 2);
    shader->setUniform(shader->getUniformLocation(RetainedString::fromStaticLiteral("fg_albedo")), 3);
    shader->setUniform(shader->getUniformLocation(RetainedString::fromStaticLiteral("fg_emissive")), 4);
    shader->setUniform(shader->getUniformLocation(RetainedString::fromStaticLiteral("fg_bump")), 5);

    const int mvpUniform = shader->getUniformLocation(RetainedString::fromStaticLiteral("mvp"));
    if (mvpUniform != -1) shader->setUniform(mvpUniform, nicxlive::core::math::Mat4::identity());
    const int offsetUniform = shader->getUniformLocation(RetainedString::fromStaticLiteral("offset"));
    if (offsetUniform != -1) shader->setUniform(offsetUniform, nicxlive::core::math::Vec2{0.0f, 0.0f});

    glDrawArrays(GL_TRIANGLES, 0, 6);
    for (int i = 5; i >= 0; --i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void oglSetAdvancedBlendCoherent(bool enable) {
    if (enable) glEnable(GL_BLEND_ADVANCED_COHERENT_KHR);
    else glDisable(GL_BLEND_ADVANCED_COHERENT_KHR);
}

void oglSetLegacyBlendMode(BlendMode blendingMode) {
    switch (blendingMode) {
        case BlendMode::Normal:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::Multiply:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::Screen:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
            break;
        case BlendMode::Lighten:
            glBlendEquation(GL_MAX);
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        case BlendMode::ColorDodge:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_DST_COLOR, GL_ONE);
            break;
        case BlendMode::LinearDodge:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::AddGlow:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::Subtract:
            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
            glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
            break;
        case BlendMode::Exclusion:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE);
            break;
        case BlendMode::Inverse:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::DestinationIn:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
            break;
        case BlendMode::ClipToLower:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::SliceFromLower:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
            break;
        default:
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
}

void oglSetAdvancedBlendEquation(BlendMode blendingMode) {
    switch (blendingMode) {
        case BlendMode::Multiply: glBlendEquation(GL_MULTIPLY_KHR); break;
        case BlendMode::Screen: glBlendEquation(GL_SCREEN_KHR); break;
        case BlendMode::Overlay: glBlendEquation(GL_OVERLAY_KHR); break;
        case BlendMode::Darken: glBlendEquation(GL_DARKEN_KHR); break;
        case BlendMode::Lighten: glBlendEquation(GL_LIGHTEN_KHR); break;
        case BlendMode::ColorDodge: glBlendEquation(GL_COLORDODGE_KHR); break;
        case BlendMode::ColorBurn: glBlendEquation(GL_COLORBURN_KHR); break;
        case BlendMode::HardLight: glBlendEquation(GL_HARDLIGHT_KHR); break;
        case BlendMode::SoftLight: glBlendEquation(GL_SOFTLIGHT_KHR); break;
        case BlendMode::Difference: glBlendEquation(GL_DIFFERENCE_KHR); break;
        case BlendMode::Exclusion: glBlendEquation(GL_EXCLUSION_KHR); break;
        default:
            oglSetLegacyBlendMode(blendingMode);
            break;
    }
}

void oglIssueBlendBarrier() {
    glBlendBarrierKHR();
}


bool oglSupportsAdvancedBlend() { return SDL_GL_ExtensionSupported("GL_KHR_blend_equation_advanced"); }
bool oglSupportsAdvancedBlendCoherent() { return SDL_GL_ExtensionSupported("GL_KHR_blend_equation_advanced_coherent"); }
#else
std::shared_ptr<Shader> oglGetBlendShader(BlendMode) { return nullptr; }
void oglBlendToBuffer(std::shared_ptr<Shader>, BlendMode, unsigned,
    unsigned, unsigned, unsigned, unsigned, unsigned, unsigned) {}
void oglSetAdvancedBlendCoherent(bool) {}
void oglSetLegacyBlendMode(BlendMode) {}
void oglSetAdvancedBlendEquation(BlendMode) {}
void oglIssueBlendBarrier() {}
bool oglSupportsAdvancedBlend() { return false; }
bool oglSupportsAdvancedBlendCoherent() { return false; }
#endif

void oglInitBlending() { nicxlive::core::nodes::inInitBlending(); }
void oglSetTripleBufferFallback(bool enabled) { nicxlive::core::nodes::nlSetTripleBufferFallback(enabled); }
bool oglIsTripleBufferFallbackEnabled() { return nicxlive::core::nodes::nlIsTripleBufferFallbackEnabled(); }
void oglApplyBlendingCapabilities() { nicxlive::core::nodes::nlApplyBlendingCapabilities(); }
bool oglUseMultistageBlending(BlendMode mode) { return nicxlive::core::nodes::inUseMultistageBlending(mode); }
void oglSetBlendMode(BlendMode mode, bool legacyOnly) { nicxlive::core::nodes::inSetBlendMode(mode, legacyOnly); }
void oglBlendModeBarrier(BlendMode mode) { nicxlive::core::nodes::inBlendModeBarrier(mode); }
}
