#pragma once
#include <texture.hpp>
#include <cstddef>
#include <cstdint>
#include <span>

namespace nijigenerate::core::texture_pixels {
// Existing native premultiplication kernel: mutate complete pixels in place.
// Source numeric correspondence remains tracked by the M198 integration scope.
inline void ngTexPremultiply(std::span<std::uint8_t> data, int channels = 4) {
    if (channels < 4) return;
    const auto size = data.size();
    const auto stride = static_cast<std::size_t>(channels);
    for (std::size_t i = 0; i < size / stride; ++i) {
        const auto offset = i * stride;
        const auto alpha = static_cast<int>(data[offset + 3]);
        data[offset] = static_cast<std::uint8_t>((static_cast<int>(data[offset]) * alpha) / 255);
        data[offset + 1] = static_cast<std::uint8_t>((static_cast<int>(data[offset + 1]) * alpha) / 255);
        data[offset + 2] = static_cast<std::uint8_t>((static_cast<int>(data[offset + 2]) * alpha) / 255);
    }
}
inline void ngTexPremultiply(nicxlive::core::TextureData& data, int channels = 4) {
    if (channels < 4) return;
    ngTexPremultiply(data ? std::span<std::uint8_t>(*data) : std::span<std::uint8_t>{}, channels);
}
inline void ngTexPremultiply(nicxlive::core::SourceTextureBytes data, int channels = 4) {
    ngTexPremultiply(std::span<std::uint8_t>(data.data(), data.size()), channels);
}
}
