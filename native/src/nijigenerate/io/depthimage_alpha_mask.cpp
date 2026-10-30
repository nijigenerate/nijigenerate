#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <cmath>
#include <new>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::io::depthimage {
namespace {
template<class Bytes> std::uint8_t readByte(Bytes bytes, int index, std::size_t sourceLine) {
    const auto offset = static_cast<std::size_t>(index);
    if (offset >= bytes.size()) throw compat::ngStaticError<compat::ArrayIndexError>(offset, bytes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), sourceLine);
    return bytes[offset];
}
template<class Bytes> std::uint8_t& resultByte(Bytes& bytes, int index, std::size_t sourceLine) {
    const auto offset = static_cast<std::size_t>(index);
    if (offset >= bytes.size()) throw compat::ngStaticError<compat::ArrayIndexError>(offset, bytes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), sourceLine);
    return bytes[offset];
}

template<class Output, class Input> Output maskFromAlphaGreaterThan(Input alpha, std::uint8_t minAlpha) {
    auto result = detail::initialByteArray<Output>(alpha.size());
    for (std::size_t i = 0; i < alpha.size(); ++i) result[i] = alpha[i] > minAlpha ? 1 : 0;
    return result;
}

template<class Output, class Input> Output scaleAlphaMask(Input sourceMask,
        int sourceWidth, int sourceHeight, int width, int height) {
    using namespace depthsample::detail;
    if (sourceMask.size() != static_cast<std::size_t>(mul32(sourceWidth, sourceHeight)))
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Source mask length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 605);
    if (sourceWidth < 0 || sourceHeight < 0 || width < 0 || height < 0)
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Mask dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 606);
    auto result = detail::initialByteArray<Output>(static_cast<std::size_t>(mul32(width, height)));
    if (sourceWidth == 0 || sourceHeight == 0 || width == 0 || height == 0) return result;
    for (int y = 0; y < height; ++y) {
        const int sourceY = std::min(sourceHeight - 1, std::max(0, compat::ngFloatToInt32(
            std::floor((static_cast<double>(y) + 0.5) * static_cast<double>(sourceHeight)
                / static_cast<double>(height)))));
        for (int x = 0; x < width; ++x) {
            const int sourceX = std::min(sourceWidth - 1, std::max(0, compat::ngFloatToInt32(
                std::floor((static_cast<double>(x) + 0.5) * static_cast<double>(sourceWidth)
                    / static_cast<double>(width)))));
            auto& destination = resultByte(result, add32(mul32(y, width), x), 618);
            const auto value = readByte(sourceMask, add32(mul32(sourceY, sourceWidth), sourceX), 618);
            destination = value ? 1 : 0;
        }
    }
    return result;
}

template<class Output, class Alpha, class Mask> Output applyPsdMaskToAlpha(Alpha alpha,
        int width, int height, int layerLeft, int layerTop, Mask mask,
        int maskWidth, int maskHeight, int maskLeft, int maskTop,
        bool positionRelativeToLayer, std::uint8_t defaultMask) {
    using namespace depthsample::detail;
    if (alpha.size() != static_cast<std::size_t>(mul32(width, height)))
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Alpha length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 577);
    if (mask.size() != static_cast<std::size_t>(mul32(maskWidth, maskHeight)))
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Mask length must match dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 578);
    Output result;
    if constexpr (std::same_as<Output, compat::Slice<std::uint8_t>>) {
        if (!alpha.empty()) {
            try {
                result = Output(alpha.begin(), alpha.end());
            } catch (const compat::OutOfMemoryError&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            }
        }
    } else {
        result.resize(alpha.size());
        for (std::size_t i = 0; i < alpha.size(); ++i) result[i] = alpha[i];
    }
    const int localMaskLeft = positionRelativeToLayer ? maskLeft : sub32(maskLeft, layerLeft);
    const int localMaskTop = positionRelativeToLayer ? maskTop : sub32(maskTop, layerTop);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int maskX = sub32(x, localMaskLeft), maskY = sub32(y, localMaskTop);
            auto maskValue = defaultMask;
            if (maskX >= 0 && maskX < maskWidth && maskY >= 0 && maskY < maskHeight)
                maskValue = readByte(mask, add32(mul32(maskY, maskWidth), maskX), 589);
            auto& value = resultByte(result, add32(mul32(y, width), x), 591);
            const int rounded = static_cast<int>(std::lround((static_cast<double>(value) * maskValue) / 255.0));
            value = static_cast<std::uint8_t>(std::min(255, std::max(0, rounded)));
        }
    }
    return result;
}
} // namespace

std::vector<std::uint8_t> ngDepthDrawMaskFromAlphaGreaterThan(
        std::span<const std::uint8_t> alpha, std::uint8_t minAlpha) {
    return maskFromAlphaGreaterThan<std::vector<std::uint8_t>>(alpha, minAlpha);
}

compat::Slice<std::uint8_t> ngDepthDrawMaskFromAlphaGreaterThanSource(
        compat::ConstSlice<std::uint8_t> alpha, std::uint8_t minAlpha) {
    return maskFromAlphaGreaterThan<compat::Slice<std::uint8_t>>(alpha, minAlpha);
}

std::vector<std::uint8_t> ngDepthDrawScaleAlphaMask(std::span<const std::uint8_t> sourceMask,
        int sourceWidth, int sourceHeight, int width, int height) {
    return scaleAlphaMask<std::vector<std::uint8_t>>(sourceMask, sourceWidth, sourceHeight, width, height);
}

compat::Slice<std::uint8_t> ngDepthDrawScaleAlphaMaskSource(compat::ConstSlice<std::uint8_t> sourceMask,
        int sourceWidth, int sourceHeight, int width, int height) {
    return scaleAlphaMask<compat::Slice<std::uint8_t>>(sourceMask, sourceWidth, sourceHeight, width, height);
}

std::vector<std::uint8_t> ngDepthDrawApplyPsdMaskToAlpha(std::span<const std::uint8_t> alpha,
        int width, int height, int layerLeft, int layerTop, std::span<const std::uint8_t> mask,
        int maskWidth, int maskHeight, int maskLeft, int maskTop,
        bool positionRelativeToLayer, std::uint8_t defaultMask) {
    return applyPsdMaskToAlpha<std::vector<std::uint8_t>>(alpha, width, height, layerLeft, layerTop,
        mask, maskWidth, maskHeight, maskLeft, maskTop, positionRelativeToLayer, defaultMask);
}

compat::Slice<std::uint8_t> ngDepthDrawApplyPsdMaskToAlphaSource(compat::ConstSlice<std::uint8_t> alpha,
        int width, int height, int layerLeft, int layerTop, compat::ConstSlice<std::uint8_t> mask,
        int maskWidth, int maskHeight, int maskLeft, int maskTop,
        bool positionRelativeToLayer, std::uint8_t defaultMask) {
    return applyPsdMaskToAlpha<compat::Slice<std::uint8_t>>(alpha, width, height, layerLeft, layerTop,
        mask, maskWidth, maskHeight, maskLeft, maskTop, positionRelativeToLayer, defaultMask);
}

} // namespace nijigenerate::io::depthimage
