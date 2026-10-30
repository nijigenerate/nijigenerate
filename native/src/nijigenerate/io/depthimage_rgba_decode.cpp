#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/throwable.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace nijigenerate::io::depthimage {

namespace {
compat::Slice<std::uint8_t> decodePixels(std::span<const std::uint8_t> rgba, DepthImageChannel channel) {
    if (rgba.size() % 4 != 0) throw compat::ThrowableException(
        detail::copyEnforceMessage("RGBA buffer length must be divisible by 4"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 429);
    auto result = detail::initialByteArray<compat::Slice<std::uint8_t>>(rgba.size() / 4);
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto offset = i * 4;
        if (rgba[offset + 3] == 0) {
            result[i] = 0;
            continue;
        }
        const auto depth01 = depthsample::ngDepthSamplePixelDepth01(rgba, offset, channel, false);
        result[i] = static_cast<std::uint8_t>(std::min(255,
            std::max(0, static_cast<int>(std::lround(depth01 * 255.0f)))));
    }
    return result;
}

compat::Slice<std::uint8_t> alphaMask(std::span<const std::uint8_t> rgba, std::uint8_t minAlpha) {
    if (rgba.size() % 4 != 0) throw compat::ThrowableException(
        detail::copyEnforceMessage("RGBA buffer length must be divisible by 4"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 445);
    auto mask = detail::initialByteArray<compat::Slice<std::uint8_t>>(rgba.size() / 4);
    for (std::size_t i = 0; i < mask.size(); ++i) mask[i] = rgba[i * 4 + 3] >= minAlpha ? 1 : 0;
    return mask;
}
std::vector<std::uint8_t> nativeVector(compat::Slice<std::uint8_t> values) {
    if (values.empty()) return {};
    return {values.begin(), values.end()};
}
}

compat::Slice<std::uint8_t> ngDepthDrawDecodeDepthPixelsFromRgbaSource(
    compat::ConstSlice<std::uint8_t> rgba, DepthImageChannel channel) {
    return decodePixels({rgba.data(), rgba.size()}, channel);
}
compat::Slice<std::uint8_t> ngDepthDrawAlphaMaskFromRgbaSource(
    compat::ConstSlice<std::uint8_t> rgba, std::uint8_t minAlpha) {
    return alphaMask({rgba.data(), rgba.size()}, minAlpha);
}
std::vector<std::uint8_t> ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba(std::span<const std::uint8_t> rgba) {
    return ngDepthDrawDecodeDepthPixelsFromRgba(rgba, DepthImageChannel::AverageRGB);
}
compat::Slice<std::uint8_t> ngDepthDrawDecodeGrayscaleDepthPixelsFromRgbaSource(
    compat::ConstSlice<std::uint8_t> rgba) {
    return ngDepthDrawDecodeDepthPixelsFromRgbaSource(rgba, DepthImageChannel::AverageRGB);
}
std::vector<std::uint8_t> ngDepthDrawDecodeDepthPixelsFromRgba(
    std::span<const std::uint8_t> rgba, DepthImageChannel channel) {
    return nativeVector(decodePixels(rgba, channel));
}
std::vector<std::uint8_t> ngDepthDrawAlphaMaskFromRgba(std::span<const std::uint8_t> rgba, std::uint8_t minAlpha) {
    return nativeVector(alphaMask(rgba, minAlpha));
}

} // namespace nijigenerate::io::depthimage
