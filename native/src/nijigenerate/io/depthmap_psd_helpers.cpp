#include "depthmap_psd_helpers.hpp"
#include "depthmap_psd_math.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <algorithm>
#include <bit>
namespace nijigenerate::io::depthmap_psd::detail {
namespace {
template<class Array> decltype(auto) arrayAtSource(Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), line);
    return values[index];
}
}
depthsample::DepthSampleChannel sampleChannel(PsdDepthChannel channel) {
    switch (channel) {
    case PsdDepthChannel::AverageRGB: return depthsample::DepthSampleChannel::AverageRGB;
    case PsdDepthChannel::R: return depthsample::DepthSampleChannel::R;
    case PsdDepthChannel::G: return depthsample::DepthSampleChannel::G;
    case PsdDepthChannel::B: return depthsample::DepthSampleChannel::B;
    case PsdDepthChannel::Luminance: return depthsample::DepthSampleChannel::Luminance;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 1121);
}
depthsample::DepthSampleConvolution sampleConvolution(PsdDepthConvolution convolution) {
    switch (convolution) {
#define NG_PSD_CONV(Name) case PsdDepthConvolution::Name: return depthsample::DepthSampleConvolution::Name;
    NG_PSD_CONV(Nearest) NG_PSD_CONV(Box3x3) NG_PSD_CONV(Box5x5)
    NG_PSD_CONV(Gaussian3x3) NG_PSD_CONV(Gaussian5x5) NG_PSD_CONV(Median3x3)
    NG_PSD_CONV(Frontmost3x3) NG_PSD_CONV(Backmost3x3) NG_PSD_CONV(BoxCustom)
    NG_PSD_CONV(GaussianCustom) NG_PSD_CONV(MedianCustom) NG_PSD_CONV(FrontmostCustom)
    NG_PSD_CONV(BackmostCustom)
#undef NG_PSD_CONV
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 1136);
}
float clamp01(float value) { return std::max(0.0f, std::min(1.0f, value)); }
compat::Slice<std::uint8_t> cropRgbaRect(std::span<const std::uint8_t> rgba,
    int sourceWidth, int sourceHeight, int left, int top, int width, int height) {
    compat::Slice<std::uint8_t> result;
    if (width <= 0 || height <= 0) return result;
    result.resize(std::size_t(width) * std::size_t(height) * 4);
    for (int y = 0; y < height; ++y) {
        const int sourceY = depthsample::detail::add32(top, y);
        for (int x = 0; x < width; ++x) {
            const int sourceX = depthsample::detail::add32(left, x);
            const auto output = (std::size_t(y) * std::size_t(width) + std::size_t(x)) * 4;
            if (sourceX < 0 || sourceY < 0 || sourceX >= sourceWidth || sourceY >= sourceHeight) {
                arrayAtSource(result, output + 3, 852) = 0; continue;
            }
            const auto input = (std::size_t(sourceY) * std::size_t(sourceWidth) + std::size_t(sourceX)) * 4;
            if (input + 3 >= rgba.size()) { arrayAtSource(result, output + 3, 857) = 0; continue; }
            const auto outputEnd = output + 4;
            if (output > outputEnd || outputEnd > result.size())
                throw compat::ngStaticError<compat::ArraySliceError>(output, outputEnd, result.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 860);
            auto destination = result.slice(output, outputEnd);
            const auto inputEnd = input + 4;
            if (input > inputEnd || inputEnd > rgba.size())
                throw compat::ngStaticError<compat::ArraySliceError>(
                    input, inputEnd, rgba.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 860);
            std::copy_n(rgba.data() + input, 4, destination.data());
        }
    }
    return result;
}
compat::Slice<std::uint8_t> normalizeFlatDepthRgba(std::span<const std::uint8_t> rgba,
    int width, int height, int channels) {
    const auto count = std::size_t(std::max(0, depthsample::detail::mul32(width, height)));
    compat::Slice<std::uint8_t> result;
    result.resize(count * 4);
    const int sourceChannels = std::max(1, channels);
    const auto sourceLength = std::min(rgba.size(), count * std::size_t(sourceChannels));
    for (std::size_t i = 0; i < count; ++i) {
        const auto input = i * std::size_t(sourceChannels), output = i * 4;
        if (input >= sourceLength) { arrayAtSource(result, output + 3, 877) = 0; continue; }
        switch (sourceChannels) {
        case 1:
            arrayAtSource(result, output, 882) = arrayAtSource(rgba, input, 882);
            arrayAtSource(result, output + 1, 883) = arrayAtSource(rgba, input, 883);
            arrayAtSource(result, output + 2, 884) = arrayAtSource(rgba, input, 884);
            arrayAtSource(result, output + 3, 885) = 255;
            break;
        case 2:
            arrayAtSource(result, output, 888) = arrayAtSource(rgba, input, 888);
            arrayAtSource(result, output + 1, 889) = arrayAtSource(rgba, input, 889);
            arrayAtSource(result, output + 2, 890) = arrayAtSource(rgba, input, 890);
            arrayAtSource(result, output + 3, 891) = input + 1 < sourceLength
                ? arrayAtSource(rgba, input + 1, 891) : 255;
            break;
        case 3:
            arrayAtSource(result, output, 894) = arrayAtSource(rgba, input, 894);
            arrayAtSource(result, output + 1, 895) = input + 1 < sourceLength
                ? arrayAtSource(rgba, input + 1, 895) : arrayAtSource(rgba, input, 895);
            arrayAtSource(result, output + 2, 896) = input + 2 < sourceLength
                ? arrayAtSource(rgba, input + 2, 896) : arrayAtSource(rgba, input, 896);
            arrayAtSource(result, output + 3, 897) = 255;
            break;
        default:
            arrayAtSource(result, output, 900) = arrayAtSource(rgba, input, 900);
            arrayAtSource(result, output + 1, 901) = input + 1 < sourceLength
                ? arrayAtSource(rgba, input + 1, 901) : arrayAtSource(rgba, input, 901);
            arrayAtSource(result, output + 2, 902) = input + 2 < sourceLength
                ? arrayAtSource(rgba, input + 2, 902) : arrayAtSource(rgba, input, 902);
            arrayAtSource(result, output + 3, 903) = input + 3 < sourceLength
                ? arrayAtSource(rgba, input + 3, 903) : 255;
            break;
        }
    }
    return result;
}
compat::Slice<std::uint8_t> flipRgbaY(std::span<const std::uint8_t> rgba, int width, int height) {
    const auto expected = std::size_t(std::max(0, depthsample::detail::mul32(width, height))) * 4;
    compat::Slice<std::uint8_t> result;
    result.resize(expected);
    if (rgba.size() < expected || width <= 0 || height <= 0) return result;
    const auto rowBytes = std::size_t(width) * 4;
    for (int y = 0; y < height; ++y) {
        const auto destination = std::size_t(height - 1 - y) * rowBytes;
        const auto upper = destination + rowBytes;
        if (destination > upper || upper > result.size())
            throw compat::ngStaticError<compat::ArraySliceError>(
                destination, upper, result.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 919);
        auto row = result.slice(destination, upper);
        std::copy_n(rgba.data() + std::size_t(y) * rowBytes, rowBytes, row.data());
    }
    return result;
}
float compositeSourceLayerMatchScore(PsdDepthCompositeSourceLayer color, PsdDepthCompositeSourceLayer depth) {
    if (!color.id.empty() && color.id == depth.id) return 4.0f;
    if (!color.name.empty() && !depth.name.empty()) {
        const auto colorKey = ngPsdDepthLayerIdentityKey(color.name, color.left, color.top, color.width, color.height);
        const auto depthKey = ngPsdDepthLayerIdentityKey(depth.name, depth.left, depth.top, depth.width, depth.height);
        if (colorKey == depthKey) return 3.0f;
    }
    if (!color.name.empty() && !depth.name.empty()) {
        const auto colorName = ngPsdDepthNormalizeLayerName(color.name);
        const auto depthName = ngPsdDepthNormalizeLayerName(depth.name);
        if (colorName == depthName) return 2.0f;
    }
    const float overlap = ngPsdDepthRectOverlapScore(float(color.left), float(color.top), float(color.width),
        float(color.height), float(depth.left), float(depth.top), float(depth.width), float(depth.height));
    return overlap > 0.0f ? overlap : 0.0f;
}
std::ptrdiff_t bestMatchingDepthSourceLayer(PsdDepthCompositeSourceLayer color,
    compat::Slice<PsdDepthCompositeSourceLayer> depths,
    const nicxlive::core::common::SharedArray<bool> reserved) {
    std::ptrdiff_t best = -1;
    float bestScore = 0.0f;
    for (std::size_t i = 0; i < depths.size(); ++i) {
        const auto depth = depths[i];
        if (i < reserved.size() && reserved[i]) continue;
        const float score = compositeSourceLayerMatchScore(color, depth);
        if (score > bestScore) { bestScore = score; best = std::bit_cast<std::ptrdiff_t>(i); }
    }
    return best;
}
}
