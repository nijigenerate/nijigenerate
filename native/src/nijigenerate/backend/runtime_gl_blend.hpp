#pragma once
#include <shader.hpp>
#include <nodes/common.hpp>
namespace nijigenerate::backend::runtime_gl {
using BlendMode = nicxlive::core::nodes::BlendMode;
std::shared_ptr<nicxlive::core::Shader> oglGetBlendShader(BlendMode mode);
void oglBlendToBuffer(std::shared_ptr<nicxlive::core::Shader> shader, BlendMode mode, unsigned target,
    unsigned bgAlbedo, unsigned bgEmissive, unsigned bgBump,
    unsigned fgAlbedo, unsigned fgEmissive, unsigned fgBump);
void oglSetAdvancedBlendCoherent(bool enable);
void oglSetLegacyBlendMode(BlendMode mode);
void oglSetAdvancedBlendEquation(BlendMode mode);
void oglIssueBlendBarrier();
bool oglSupportsAdvancedBlend();
bool oglSupportsAdvancedBlendCoherent();
// Independent nodes/common.d capability state used by the GL packet renderer.
void oglInitBlending();
void oglApplyBlendingCapabilities();
void oglSetTripleBufferFallback(bool enable);
bool oglIsTripleBufferFallbackEnabled();
bool oglUseMultistageBlending(BlendMode mode);
void oglSetBlendMode(BlendMode mode, bool legacyOnly = false);
void oglBlendModeBarrier(BlendMode mode);
}
