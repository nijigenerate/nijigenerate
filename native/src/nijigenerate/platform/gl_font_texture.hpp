#pragma once
#include <cstdint>

namespace nijigenerate::platform {
std::int32_t ngGLUploadFontTexture(std::uint32_t& texture, const unsigned char* pixels, int width, int height);
void ngGLRestoreFontTextureBinding(std::int32_t binding);
void ngGLDeleteFontTexture(std::uint32_t texture);
} // namespace nijigenerate::platform
