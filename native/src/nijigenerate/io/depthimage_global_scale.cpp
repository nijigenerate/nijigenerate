#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"

#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/throwable.hpp>

namespace nijigenerate::io::depthimage {

DepthDrawGlobalDepthScaleResult ngDepthDrawApplyGlobalDepthScale(
        compat::ConstSlice<std::uint8_t> pixels, double scale, compat::ConstSlice<std::uint8_t> maskPixels) {
    return ngDepthDrawApplyGlobalDepthScale(pixels.view(), scale, maskPixels.view());
}

DepthDrawGlobalDepthScaleResult ngDepthDrawApplyGlobalDepthScale(
        std::span<const std::uint8_t> pixels, double scale, std::span<const std::uint8_t> maskPixels) {
    if (!maskPixels.empty() && maskPixels.size() != pixels.size())
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Depth mask length must match pixels"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 536);

    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const auto depth = compat::ngArrayAt(pixels, i);
        if (!maskPixels.empty() && !compat::ngArrayAt(maskPixels, i)) continue;
        if (depth == 0) continue;
        sum += depth;
        ++count;
    }

    DepthDrawGlobalDepthScaleResult result;
    result.pixels = detail::initialByteArray<decltype(result.pixels)>(pixels.size());
    result.centroid = count > 0 ? sum / static_cast<double>(count) : 0.0;
    result.count = count;
    if (count == 0) return result;

    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const auto depth = compat::ngArrayAt(pixels, i);
        if (!maskPixels.empty() && !compat::ngArrayAt(maskPixels, i)) {
            compat::ngArrayAt(result.pixels, i) = 0;
            continue;
        }
        compat::ngArrayAt(result.pixels, i) = ngDepthDrawScaleDepthValueAroundCenter(depth, scale, result.centroid);
    }
    return result;
}

} // namespace nijigenerate::io::depthimage
