#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace nijigenerate::io::depthimage {
namespace {

static_assert(sizeof(int) == sizeof(std::uint32_t));
int add32(int a, int b) { return std::bit_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }
int sub32(int a, int b) { return std::bit_cast<int>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b)); }
int mul32(int a, int b) { return std::bit_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
template<class Bytes> std::uint8_t checkedByte(Bytes bytes, int index, std::size_t sourceLine) {
    const auto offset = static_cast<std::size_t>(index);
    if (offset >= bytes.size()) throw compat::ngStaticError<compat::ArrayIndexError>(offset, bytes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), sourceLine);
    return bytes[offset];
}

std::uint8_t roundedDepth(double value) {
    // Current D/Windows lround(real) delegates to the 64-bit CRT llround(double).
    // Preserve its subsequent explicit D int cast before byte clamping.
    const auto rounded = std::llround(value);
    const auto narrowed = std::bit_cast<int>(static_cast<std::uint32_t>(rounded));
    return static_cast<std::uint8_t>(std::min(255, std::max(1, narrowed)));
}

} // namespace

namespace {
template<class Output, class Input> Output sampleDepthLayerToColorLayer(Input depthPixels,
        int depthWidth, int depthHeight, int depthLeft, int depthTop, Input depthAlphaMask,
        int colorWidth, int colorHeight, int colorLeft, int colorTop) {
    if (depthPixels.size() != static_cast<std::size_t>(mul32(depthWidth, depthHeight)))
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Depth pixels length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 487);
    if (!depthAlphaMask.empty() && depthAlphaMask.size() != depthPixels.size())
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Depth alpha mask length must match depth pixels"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 488);
    auto result = detail::initialByteArray<Output>(static_cast<std::size_t>(mul32(colorWidth, colorHeight)));
    for (int y = 0; y < colorHeight; ++y) {
        const auto depthY = sub32(add32(colorTop, y), depthTop);
        if (depthY < 0 || depthY >= depthHeight) continue;
        for (int x = 0; x < colorWidth; ++x) {
            const auto depthX = sub32(add32(colorLeft, x), depthLeft);
            if (depthX < 0 || depthX >= depthWidth) continue;
            const auto localIndex = add32(mul32(y, colorWidth), x);
            const auto depthIndex = add32(mul32(depthY, depthWidth), depthX);
            if (!depthAlphaMask.empty() && checkedByte(depthAlphaMask, depthIndex, 503) == 0) continue;
            const auto outputIndex = static_cast<std::size_t>(localIndex);
            if (outputIndex >= result.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                outputIndex, result.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 504);
            auto& destination = result[outputIndex];
            destination = checkedByte(depthPixels, depthIndex, 504);
        }
    }
    return result;
}
} // namespace

std::vector<std::uint8_t> ngDepthDrawSampleDepthLayerToColorLayer(std::span<const std::uint8_t> depthPixels,
        int depthWidth, int depthHeight, int depthLeft, int depthTop, std::span<const std::uint8_t> depthAlphaMask,
        int colorWidth, int colorHeight, int colorLeft, int colorTop) {
    return sampleDepthLayerToColorLayer<std::vector<std::uint8_t>>(depthPixels,
        depthWidth, depthHeight, depthLeft, depthTop, depthAlphaMask,
        colorWidth, colorHeight, colorLeft, colorTop);
}

compat::Slice<std::uint8_t> ngDepthDrawSampleDepthLayerToColorLayerSource(
        compat::ConstSlice<std::uint8_t> depthPixels, int depthWidth, int depthHeight,
        int depthLeft, int depthTop, compat::ConstSlice<std::uint8_t> depthAlphaMask,
        int colorWidth, int colorHeight, int colorLeft, int colorTop) {
    return sampleDepthLayerToColorLayer<compat::Slice<std::uint8_t>>(depthPixels,
        depthWidth, depthHeight, depthLeft, depthTop, depthAlphaMask,
        colorWidth, colorHeight, colorLeft, colorTop);
}

std::uint8_t ngDepthDrawScaleDepthValueAroundCenter(std::uint8_t depth, double scale, double center) {
    if (depth == 0) return 0;
    if (!std::isfinite(scale) || scale == 1.0 || center <= 0.0) return roundedDepth(depth);
    return roundedDepth(center + (static_cast<double>(depth) - center) * scale);
}

} // namespace nijigenerate::io::depthimage
