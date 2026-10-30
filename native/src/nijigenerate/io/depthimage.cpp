#include "depthimage.hpp"
#include "depthimage_internal_types.hpp"
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/throwable.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

namespace nijigenerate::io::depthimage {
namespace {
// These views name process-lifetime literals returned directly by the source API.
constexpr std::array<std::string_view, 5> channelNames{"AverageRGB", "R", "G", "B", "Luminance"};
constexpr std::array<std::string_view, 13> convolutionNames{
    "Nearest", "Box3x3", "Box5x5", "Gaussian3x3", "Gaussian5x5", "Median3x3", "Frontmost3x3",
    "Backmost3x3", "BoxCustom", "GaussianCustom", "MedianCustom", "FrontmostCustom", "BackmostCustom"
};

// MSVC/LDC's x64 signed float conversion returns INT_MIN for non-finite and
// out-of-range inputs. Avoid undefined C++ floating-to-integer conversion.
int floatToInt(float value) {
    if (!std::isfinite(value) || value < -2147483648.0f || value >= 2147483648.0f) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }
float clamp01(float value) { return std::max(0.0f, std::min(1.0f, value)); }

compat::RetainedString concatenateEnumError(std::string_view prefix, std::string_view value) {
    // _d_arraycatnTX totals lengths before allocating and copying its operands.
    const auto length = prefix.size() + value.size();
    if (length == 0) return {};
    auto bytes = detail::initialPrimitiveArray<char>(length, static_cast<char>(0xff), 1, 970, 1000);
    if (!prefix.empty()) std::memcpy(bytes.data(), prefix.data(), prefix.size());
    if (!value.empty()) std::memcpy(bytes.data() + prefix.size(), value.data(), value.size());
    return compat::RetainedString::retainedView(bytes.retentionOwner(), bytes.data(), bytes.size());
}

float coverageAlphaPixel(std::span<const std::uint8_t> rgba,
    int width, int height, int coverageChannels, int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) return 0.0f;
    const auto channels = std::max(1, coverageChannels);
    const auto index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x)) * static_cast<std::size_t>(channels);
    if (channels < 4 || index + 3 >= rgba.size()) return 1.0f;
    return static_cast<float>(rgba[index + 3]) / 255.0f;
}

DepthImageSampleResult sampleRgbaWithCoverage(std::span<const std::uint8_t> rgba,
    int width, int height, std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float coverageOpacity, float x, float y, float layerOpacity, DepthImageSampleSettings settings) {
    if (width < 0 || height < 0) throw compat::ThrowableException(
        detail::copyEnforceMessage("Image dimensions must be non-negative"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 257);
    if (coverageWidth < 0 || coverageHeight < 0) {
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Coverage dimensions must be non-negative"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 258);
    }
    if (rgba.size() < static_cast<std::size_t>(std::max(0, depthsample::detail::mul32(width, height))) * 4) {
        throw compat::ThrowableException(
            detail::copyEnforceMessage("RGBA buffer is smaller than dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 259);
    }
    if (coverageRgba.size() < static_cast<std::size_t>(
            std::max(0, depthsample::detail::mul32(coverageWidth, coverageHeight))) * 4) {
        throw compat::ThrowableException(
            detail::copyEnforceMessage("Coverage RGBA buffer is smaller than dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 260);
    }
    const auto centerX = floatToInt(std::round(x));
    const auto centerY = floatToInt(std::round(y));
    const auto opacity = clamp01(layerOpacity);
    const auto coverageLayerOpacity = clamp01(coverageOpacity);
    auto sampleAt = [&](int sampleX, int sampleY) -> depthsample::DepthSamplePoint {
        if (sampleX < 0 || sampleY < 0 || sampleX >= width || sampleY >= height) {
            return depthsample::ngDepthSampleMissingPoint();
        }
        if (!ngDepthImageCoverageReliableAt(sampleX, sampleY, width, height,
                coverageRgba, coverageWidth, coverageHeight)) return depthsample::ngDepthSampleMissingPoint();
        const auto index = (static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(sampleX)) * 4;
        const auto alpha = depthsample::ngDepthSampleEffectiveAlpha(rgba, index, opacity)
            * ngDepthImageCoverageAlphaAt(sampleX, sampleY, width, height,
                coverageRgba, coverageWidth, coverageHeight) * coverageLayerOpacity;
        if (!depthsample::ngDepthSampleAcceptsAlpha(alpha, settings.alphaThreshold)) {
            return depthsample::ngDepthSampleMissingPoint();
        }
        const auto depth = depthsample::ngDepthSamplePixelDepth(rgba, index, settings.channel,
            settings.invert, settings.backDepth, settings.frontDepth, settings.depthScale);
        return {true, depth, alpha};
    };
    const auto sample = depthsample::ngDepthSampleConvolve(sampleAt, settings.convolution,
        settings.customRadius, centerX, centerY);
    return {sample.valid, sample.value};
}
}

int ngNormalizeDepthImageCustomRadius(int radius) noexcept {
    return std::max(DepthImageMinCustomRadius, std::min(DepthImageMaxCustomRadius, radius));
}

std::string ngDepthImageChannelName(DepthImageChannel value) {
    return std::string(ngDepthImageChannelNameSource(value).view());
}
compat::RetainedString ngDepthImageChannelNameSource(DepthImageChannel value) {
    const auto index = static_cast<unsigned int>(value);
    if (index >= channelNames.size()) throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 93);
    return compat::RetainedString::fromBorrowedView(channelNames[index]);
}

DepthImageChannel ngDepthImageChannelFromString(std::string_view value) {
    for (std::size_t i = 0; i < channelNames.size(); ++i) {
        if (value == channelNames[i]) return static_cast<DepthImageChannel>(i);
    }
    auto message = concatenateEnumError("Unknown depth image channel: ", value);
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 109);
}

DepthImageChannel ngDepthImageChannelFromStringSource(compat::RetainedString value) {
    return ngDepthImageChannelFromString(value.view());
}

std::string ngDepthImageConvolutionName(DepthImageConvolution value) {
    return std::string(ngDepthImageConvolutionNameSource(value).view());
}
compat::RetainedString ngDepthImageConvolutionNameSource(DepthImageConvolution value) {
    const auto index = static_cast<unsigned int>(value);
    if (index >= convolutionNames.size()) throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 114);
    return compat::RetainedString::fromBorrowedView(convolutionNames[index]);
}

DepthImageConvolution ngDepthImageConvolutionFromString(std::string_view value) {
    for (std::size_t i = 0; i < convolutionNames.size(); ++i) {
        if (value == convolutionNames[i]) return static_cast<DepthImageConvolution>(i);
    }
    auto message = concatenateEnumError("Unknown depth image convolution: ", value);
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 146);
}

DepthImageConvolution ngDepthImageConvolutionFromStringSource(compat::RetainedString value) {
    return ngDepthImageConvolutionFromString(value.view());
}

DepthImageSampleResult ngDepthImageFrontmost(std::span<const DepthImageSampleResult> samples) {
    return depthsample::ngDepthSampleFrontmost(samples);
}
DepthImageSampleResult ngDepthImageFrontmostSource(compat::Slice<DepthImageSampleResult> samples) {
    return ngDepthImageFrontmost(std::span<const DepthImageSampleResult>{samples.data(), samples.size()});
}

namespace {
template<class Output, class Input> Output depthPixelsFromRgbaRed(Input rgba) {
    if (rgba.size() % 4 != 0) throw compat::ThrowableException(
        detail::copyEnforceMessage("RGBA buffer length must be divisible by 4"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 417);
    auto result = detail::initialByteArray<Output>(rgba.size() / 4);
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = rgba[i * 4];
    return result;
}
} // namespace

std::vector<std::uint8_t> ngDepthDrawDepthPixelsFromRgbaRed(std::span<const std::uint8_t> rgba) {
    return depthPixelsFromRgbaRed<std::vector<std::uint8_t>>(rgba);
}
compat::Slice<std::uint8_t> ngDepthDrawDepthPixelsFromRgbaRedSource(
    compat::ConstSlice<std::uint8_t> rgba) {
    return depthPixelsFromRgbaRed<compat::Slice<std::uint8_t>>(rgba);
}

float ngDepthImageCoverageAlphaAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> rgba, int coverageWidth, int coverageHeight) {
    return ngDepthImageCoverageAlphaAt(x, y, width, height, rgba, coverageWidth, coverageHeight, 4);
}

float ngDepthImageCoverageAlphaAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> rgba, int coverageWidth, int coverageHeight, int coverageChannels) {
    if (rgba.empty() || coverageWidth <= 0 || coverageHeight <= 0) return 1.0f;
    if (x < 0 || y < 0 || x >= width || y >= height) return 0.0f;
    auto coverageX = floatToInt(std::round(((static_cast<float>(x) + 0.5f)
        * static_cast<float>(coverageWidth) / static_cast<float>(width)) - 0.5f));
    auto coverageY = floatToInt(std::round(((static_cast<float>(y) + 0.5f)
        * static_cast<float>(coverageHeight) / static_cast<float>(height)) - 0.5f));
    coverageX = std::max(0, std::min(coverageWidth - 1, coverageX));
    coverageY = std::max(0, std::min(coverageHeight - 1, coverageY));
    const auto channels = std::max(1, coverageChannels);
    const auto index = (static_cast<std::size_t>(coverageY) * static_cast<std::size_t>(coverageWidth)
        + static_cast<std::size_t>(coverageX)) * static_cast<std::size_t>(channels);
    if (channels < 4 || index + 3 >= rgba.size()) return 1.0f;
    return static_cast<float>(rgba[index + 3]) / 255.0f;
}

bool ngDepthImageCoverageReliableAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> rgba, int coverageWidth, int coverageHeight) {
    return ngDepthImageCoverageReliableAt(x, y, width, height, rgba, coverageWidth, coverageHeight, 4);
}

bool ngDepthImageCoverageReliableAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> rgba, int coverageWidth, int coverageHeight, int coverageChannels) {
    return rgba.empty() || ngDepthImageCoverageAlphaAt(x, y, width, height,
        rgba, coverageWidth, coverageHeight, coverageChannels) > 0.5f;
}

float ngDepthImageCoverageAlphaAtUv(float u, float v, std::span<const std::uint8_t> rgba,
    int width, int height, int coverageChannels) {
    if (rgba.empty() || width <= 0 || height <= 0) return 1.0f;
    const auto channels = std::max(1, coverageChannels);
    if (channels < 4) return 1.0f;
    const auto clampedU = std::max(0.0f, std::min(1.0f, u));
    const auto clampedV = std::max(0.0f, std::min(1.0f, v));
    const auto x = clampedU * static_cast<float>(std::max(0, width - 1));
    const auto y = clampedV * static_cast<float>(std::max(0, height - 1));
    const auto x0 = std::max(0, std::min(width - 1, floatToInt(x)));
    const auto y0 = std::max(0, std::min(height - 1, floatToInt(y)));
    const auto x1 = std::max(0, std::min(width - 1, x0 + 1));
    const auto y1 = std::max(0, std::min(height - 1, y0 + 1));
    const auto tx = x - static_cast<float>(x0);
    const auto ty = y - static_cast<float>(y0);
    const auto a00 = coverageAlphaPixel(rgba, width, height, channels, x0, y0);
    const auto a10 = coverageAlphaPixel(rgba, width, height, channels, x1, y0);
    const auto a01 = coverageAlphaPixel(rgba, width, height, channels, x0, y1);
    const auto a11 = coverageAlphaPixel(rgba, width, height, channels, x1, y1);
    return lerp(lerp(a00, a10, tx), lerp(a01, a11, tx), ty);
}

float ngDepthImageCoverageAlphaAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> rgba, int coverageWidth, int coverageHeight) {
    return ngDepthImageCoverageAlphaAtSource(x, y, width, height, rgba, coverageWidth, coverageHeight, 4);
}

float ngDepthImageCoverageAlphaAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> rgba, int coverageWidth, int coverageHeight, int coverageChannels) {
    return ngDepthImageCoverageAlphaAt(x, y, width, height, rgba.view(),
        coverageWidth, coverageHeight, coverageChannels);
}

bool ngDepthImageCoverageReliableAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> rgba, int coverageWidth, int coverageHeight) {
    return ngDepthImageCoverageReliableAtSource(x, y, width, height, rgba, coverageWidth, coverageHeight, 4);
}

bool ngDepthImageCoverageReliableAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> rgba, int coverageWidth, int coverageHeight, int coverageChannels) {
    return ngDepthImageCoverageReliableAt(x, y, width, height, rgba.view(),
        coverageWidth, coverageHeight, coverageChannels);
}

float ngDepthImageCoverageAlphaAtUvSource(float u, float v, compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, int coverageChannels) {
    return ngDepthImageCoverageAlphaAtUv(u, v, rgba.view(), width, height, coverageChannels);
}

float ngDepthImageCompositeAlpha(float dst, float alpha) {
    dst = clamp01(dst);
    alpha = clamp01(alpha);
    return 1.0f - ((1.0f - dst) * (1.0f - alpha));
}

DepthImageSampleResult ngDepthImageSampleRgba(std::span<const std::uint8_t> rgba,
    int width, int height, float x, float y, DepthImageSampleSettings settings) {
    return ngDepthImageSampleRgbaWithOpacity(rgba, width, height, x, y, 1.0f, settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaWithOpacity(std::span<const std::uint8_t> rgba,
    int width, int height, float x, float y, float opacity, DepthImageSampleSettings settings) {
    return sampleRgbaWithCoverage(rgba, width, height, {}, 0, 0, 1.0f, x, y, clamp01(opacity), settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaWithCoverage(std::span<const std::uint8_t> rgba,
    int width, int height, std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float coverageOpacity, float x, float y, DepthImageSampleSettings settings) {
    return sampleRgbaWithCoverage(rgba, width, height, coverageRgba, coverageWidth, coverageHeight,
        coverageOpacity, x, y, 1.0f, settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaWithOpacityAndCoverage(std::span<const std::uint8_t> rgba,
    int width, int height, std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float layerOpacity, float coverageOpacity, float x, float y, DepthImageSampleSettings settings) {
    return sampleRgbaWithCoverage(rgba, width, height, coverageRgba, coverageWidth, coverageHeight,
        coverageOpacity, x, y, clamp01(layerOpacity), settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaSource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, float x, float y, DepthImageSampleSettings settings) {
    return ngDepthImageSampleRgbaWithOpacitySource(rgba, width, height, x, y, 1.0f, settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaWithOpacitySource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, float x, float y, float opacity, DepthImageSampleSettings settings) {
    return ngDepthImageSampleRgbaWithOpacity(rgba.view(), width, height, x, y, opacity, settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaWithCoverageSource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float coverageOpacity, float x, float y, DepthImageSampleSettings settings) {
    return ngDepthImageSampleRgbaWithCoverage(rgba.view(), width, height, coverageRgba.view(),
        coverageWidth, coverageHeight, coverageOpacity, x, y, settings);
}

DepthImageSampleResult ngDepthImageSampleRgbaWithOpacityAndCoverageSource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float layerOpacity, float coverageOpacity, float x, float y, DepthImageSampleSettings settings) {
    return ngDepthImageSampleRgbaWithOpacityAndCoverage(rgba.view(), width, height, coverageRgba.view(),
        coverageWidth, coverageHeight, layerOpacity, coverageOpacity, x, y, settings);
}

}
