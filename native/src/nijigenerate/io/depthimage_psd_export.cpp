#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <string_view>

namespace nijigenerate::io::depthimage {
namespace {
template<class Bytes> std::uint8_t readExportByte(Bytes bytes, int index, std::size_t sourceLine) {
    const auto offset = static_cast<std::size_t>(index);
    if (offset >= bytes.size()) throw compat::ngStaticError<compat::ArrayIndexError>(offset, bytes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), sourceLine);
    return bytes[offset];
}

template<class Output, class Input> Output psdExportDepthPixels(int sourceWidth, int sourceHeight,
        int sourceLeft, int sourceTop, Input sourceAlpha,
        int preparedLeft, int preparedTop, int preparedWidth, int preparedHeight,
        Input exportDepthPixels, Input sourceDepthPixels, Input coverageMask) {
    using namespace depthsample::detail;
    const auto count = static_cast<std::size_t>(mul32(sourceWidth, sourceHeight));
    if (sourceAlpha.size() != count)
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Source alpha length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 667);
    if (exportDepthPixels.size() != static_cast<std::size_t>(mul32(preparedWidth, preparedHeight)))
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Export depth length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 668);
    if (!sourceDepthPixels.empty() && sourceDepthPixels.size() != exportDepthPixels.size())
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Source depth length must match export depth length"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 670);
    if (!coverageMask.empty() && coverageMask.size() != exportDepthPixels.size())
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Coverage mask length must match export depth length"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 672);
    auto result = detail::initialByteArray<Output>(count);
    for (int y = 0; y < sourceHeight; ++y) {
        const auto globalY = add32(sourceTop, y);
        for (int x = 0; x < sourceWidth; ++x) {
            const auto localIndex = add32(mul32(y, sourceWidth), x);
            const auto globalX = add32(sourceLeft, x);
            const auto preparedX = sub32(globalX, preparedLeft);
            const auto preparedY = sub32(globalY, preparedTop);
            if (preparedX < 0 || preparedX >= preparedWidth || preparedY < 0 || preparedY >= preparedHeight)
                continue;
            const auto preparedIndex = add32(mul32(preparedY, preparedWidth), preparedX);
            const auto sourceDepth = !sourceDepthPixels.empty()
                ? readExportByte(sourceDepthPixels, preparedIndex, 686)
                : readExportByte(exportDepthPixels, preparedIndex, 686);
            const auto hasDepth = readExportByte(sourceAlpha, localIndex, 687) > 0
                && (coverageMask.empty() || readExportByte(coverageMask, preparedIndex, 688))
                && (readExportByte(exportDepthPixels, preparedIndex, 689) > 0 || sourceDepth > 0);
            const auto outputIndex = static_cast<std::size_t>(localIndex);
            if (outputIndex >= result.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                outputIndex, result.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 690);
            auto& destination = result[outputIndex];
            destination = hasDepth ? (readExportByte(exportDepthPixels, preparedIndex, 690)
                ? readExportByte(exportDepthPixels, preparedIndex, 690) : sourceDepth) : 0;
        }
    }
    return result;
}
} // namespace

std::vector<std::uint8_t> ngDepthDrawPsdExportDepthPixels(int sourceWidth, int sourceHeight,
        int sourceLeft, int sourceTop, std::span<const std::uint8_t> sourceAlpha,
        int preparedLeft, int preparedTop, int preparedWidth, int preparedHeight,
        std::span<const std::uint8_t> exportDepthPixels, std::span<const std::uint8_t> sourceDepthPixels,
        std::span<const std::uint8_t> coverageMask) {
    return psdExportDepthPixels<std::vector<std::uint8_t>>(sourceWidth, sourceHeight, sourceLeft, sourceTop,
        sourceAlpha, preparedLeft, preparedTop, preparedWidth, preparedHeight,
        exportDepthPixels, sourceDepthPixels, coverageMask);
}

compat::Slice<std::uint8_t> ngDepthDrawPsdExportDepthPixelsSource(int sourceWidth, int sourceHeight,
        int sourceLeft, int sourceTop, compat::ConstSlice<std::uint8_t> sourceAlpha,
        int preparedLeft, int preparedTop, int preparedWidth, int preparedHeight,
        compat::ConstSlice<std::uint8_t> exportDepthPixels, compat::ConstSlice<std::uint8_t> sourceDepthPixels,
        compat::ConstSlice<std::uint8_t> coverageMask) {
    return psdExportDepthPixels<compat::Slice<std::uint8_t>>(sourceWidth, sourceHeight, sourceLeft, sourceTop,
        sourceAlpha, preparedLeft, preparedTop, preparedWidth, preparedHeight,
        exportDepthPixels, sourceDepthPixels, coverageMask);
}
} // namespace nijigenerate::io::depthimage
