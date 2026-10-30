#pragma once
// Independent nijilive OpenGL texture_backend.d rendering implementation.
#include <texture.hpp>
#include <cstdint>
#include <span>

namespace nijigenerate::backend::runtime_gl {
using GLId = std::uint32_t;
void oglCreateTextureHandle(GLId& id);
void oglDeleteTextureHandle(GLId& id);
void oglBindTextureHandle(GLId id, unsigned unit);
void oglUploadTextureData(GLId id, int width, int height, int inChannels, int outChannels,
    bool stencil, std::span<const std::uint8_t> data);
void oglUpdateTextureRegion(GLId id, int x, int y, int width, int height, int channels,
    std::span<const std::uint8_t> data);
void oglGenerateTextureMipmap(GLId id);
void oglApplyTextureFiltering(GLId id, nicxlive::core::Filtering filtering, bool useMipmaps = true);
void oglApplyTextureWrapping(GLId id, nicxlive::core::Wrapping wrapping);
void oglApplyTextureAnisotropy(GLId id, float value);
float oglMaxTextureAnisotropy();
void oglReadTextureData(GLId id, int channels, bool stencil, std::span<std::uint8_t> buffer);
// Actual original ubyte[] value arguments retain the existing canonical allocation.
void oglUploadTextureData(GLId id, int width, int height, int inChannels, int outChannels,
    bool stencil, nicxlive::core::SourceTextureBytes data);
void oglUpdateTextureRegion(GLId id, int x, int y, int width, int height, int channels,
    nicxlive::core::SourceTextureBytes data);
void oglReadTextureData(GLId id, int channels, bool stencil, nicxlive::core::SourceTextureBytes buffer);
}
