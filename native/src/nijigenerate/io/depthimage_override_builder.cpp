#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <string_view>
#include <utility>

namespace nijigenerate::io::depthimage {
namespace {
compat::Slice<std::uint8_t> thresholdMask(compat::ConstSlice<std::uint8_t> alpha, std::uint8_t threshold) {
    return ngDepthDrawMaskFromAlphaGreaterThanSource(alpha, threshold);
}
compat::Slice<std::uint8_t> thresholdMask(std::span<const std::uint8_t> alpha, std::uint8_t threshold) {
    auto values = ngDepthDrawMaskFromAlphaGreaterThan(alpha, threshold);
    return values.empty() ? compat::Slice<std::uint8_t>{} : compat::Slice<std::uint8_t>(std::move(values));
}
template<class Input> DepthDrawDepthOverrideMasks depthOverrideMasks(Input colorAlpha, Input depthPixels) {
    if (colorAlpha.size() != depthPixels.size())
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Color alpha and depth length must match"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 632);
    DepthDrawDepthOverrideMasks result;
    result.colorMask = thresholdMask(colorAlpha, 8);
    result.strictSurfaceMask = thresholdMask(colorAlpha, 254);
    bool hasStrict = false;
    for (auto value : result.strictSurfaceMask) {
        if (value) { hasStrict = true; break; }
    }
    try {
        result.surfaceMask = (hasStrict ? result.strictSurfaceMask : result.colorMask).duplicate();
    } catch (const compat::OutOfMemoryError&) {
        // This is a byte-only clone. The canonical allocator Error must retain
        // the defining D duplication origin, rather than its native allocator.
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"),394);
    }
    result.renderDepthMask = detail::initialByteArray<decltype(result.renderDepthMask)>(depthPixels.size());
    for (std::size_t i = 0; i < depthPixels.size(); ++i)
        result.renderDepthMask[i] = result.surfaceMask[i] && depthPixels[i] > 0 ? 1 : 0;
    return result;
}
} // namespace

DepthDrawDepthOverrideMasks ngDepthDrawDepthOverrideMasks(
        std::span<const std::uint8_t> colorAlpha, std::span<const std::uint8_t> depthPixels) {
    return depthOverrideMasks(colorAlpha, depthPixels);
}

DepthDrawDepthOverrideMasks ngDepthDrawDepthOverrideMasksSource(
        compat::ConstSlice<std::uint8_t> colorAlpha, compat::ConstSlice<std::uint8_t> depthPixels) {
    return depthOverrideMasks(colorAlpha, depthPixels);
}
} // namespace nijigenerate::io::depthimage
