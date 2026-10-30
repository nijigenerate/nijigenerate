// Faithful body of regression.d7657-7863; include once outside namespaces.
#include "root_depth_image_facade.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/io/depthimage.hpp>
#include <nijigenerate/io/depthmap_psd.hpp>
#include <nijigenerate/io/depthmap_psd_enum_functions.hpp>
#include <nijigenerate/io/depthmap_psd_public_sampling.hpp>
#include <nijigenerate/io/depthsample.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace nijigenerate_tests::regression::detail {
void testDepthImageFacadeMatchesPsdDepthSampling() {
    using namespace nijigenerate::io::depthimage;
    using namespace nijigenerate::io::depthmap_psd;
    using nijigenerate::compat::ConstSlice;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ThrowableException;
    using nijigenerate::io::depthsample::DepthSampleResult;
    namespace format = nijigenerate::core::translation;
    resetCase();

    const auto sameDepthSampleValue = [](float a, float b) {
        return near(a, b) || (a != a && b != b);
    };
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    // Original std.conv.toStr(float): appender, default FormatSpec, formatValue.
    const auto floatText = [](float value) {
        std::string output;
        format::TypedFormatWriter writer(output);
        format::FormatSpec spec;
        format::ngFormatValueTo(writer, value, spec);
        return nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(std::move(output));
        });
    };
    Slice<std::uint8_t> pixels{
        0, 0, 0, 255,       255, 255, 255, 128, 0, 0, 0, 0,
        255, 0, 0, 255,     128, 128, 128, 255, 0, 255, 0, 255,
        0, 0, 255, 255,     255, 255, 255, 0,   255, 255, 255, 255,
    };
    Slice<std::uint8_t> coverage{
        255, 255, 255, 255, 255, 255, 255, 64,  255, 255, 255, 0,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 0,   255, 255, 255, 255,
    };
    DepthImageSampleSettings settings;
    settings.backDepth = -1.0f;
    settings.frontDepth = 1.0f;
    settings.alphaThreshold = 0.01f;
    settings.convolution = DepthImageConvolution::Gaussian3x3;
    settings.channel = DepthImageChannel::AverageRGB;
    auto psdSettings = ngDepthImageToPsdSettings(settings);

    for (const auto channel : Slice<DepthImageChannel>{DepthImageChannel::AverageRGB, DepthImageChannel::R,
            DepthImageChannel::G, DepthImageChannel::B, DepthImageChannel::Luminance}) {
        auto name = ngDepthImageChannelNameSource(channel);
        settings.channel = channel;
        auto converted = ngDepthImageToPsdSettings(settings);
        const bool condition = ngDepthImageChannelFromStringSource(name) == channel &&
            ngPsdDepthChannelFromString(name) == converted.channel;
        auto message = concatenateSourceStrings({
            text("neutral depth image channel names should round-trip through the PSD compatibility bridge: "), name});
        require(condition, message);
    }
    for (const auto convolution : Slice<DepthImageConvolution>{
            DepthImageConvolution::Nearest, DepthImageConvolution::Box3x3, DepthImageConvolution::Box5x5,
            DepthImageConvolution::Gaussian3x3, DepthImageConvolution::Gaussian5x5,
            DepthImageConvolution::Median3x3, DepthImageConvolution::Frontmost3x3,
            DepthImageConvolution::Backmost3x3, DepthImageConvolution::BoxCustom,
            DepthImageConvolution::GaussianCustom, DepthImageConvolution::MedianCustom,
            DepthImageConvolution::FrontmostCustom, DepthImageConvolution::BackmostCustom}) {
        auto name = ngDepthImageConvolutionNameSource(convolution);
        settings.convolution = convolution;
        auto converted = ngDepthImageToPsdSettings(settings);
        const bool condition = ngDepthImageConvolutionFromStringSource(name) == convolution &&
            ngPsdDepthConvolutionFromString(name) == converted.convolution;
        auto message = concatenateSourceStrings({
            text("neutral depth image convolution names should round-trip through the PSD compatibility bridge: "), name});
        require(condition, message);
    }
    settings.channel = DepthImageChannel::AverageRGB;
    settings.convolution = DepthImageConvolution::Gaussian3x3;
    psdSettings = ngDepthImageToPsdSettings(settings);

    auto depthImageSample = ngDepthImageSampleRgbaSource(pixels, 3, 3, 1, 1, settings);
    auto psdSample = ngPsdDepthSamplePixels(pixels, 3, 3, 1, 1, psdSettings);
    const bool sampleCondition = depthImageSample.valid == psdSample.valid &&
        sameDepthSampleValue(depthImageSample.value, psdSample.value);
    const std::array<format::TypedFormatArgument, 4> sampleArguments{
        format::ngFormatArgument(depthImageSample.valid), format::ngFormatArgument(depthImageSample.value),
        format::ngFormatArgument(psdSample.valid), format::ngFormatArgument(psdSample.value)};
    auto sampleRendered = format::ngFormatTyped(
        "neutral depth image sampling facade must match existing PSD sampling: depthImage=(%s,%s) psd=(%s,%s)",
        sampleArguments);
    auto sampleMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
        return RetainedString(std::move(sampleRendered));
    });
    require(sampleCondition, sampleMessage);

    auto depthImageCoverage = ngDepthImageSampleRgbaWithCoverageSource(
        pixels, 3, 3, coverage, 3, 3, 0.75f, 1, 1, settings);
    auto psdCoverage = ngPsdDepthSamplePixelsWithCoverage(pixels, 3, 3, coverage, 3, 3, 0.75f, 1, 1, psdSettings);
    require(depthImageCoverage.valid == psdCoverage.valid &&
        sameDepthSampleValue(depthImageCoverage.value, psdCoverage.value),
        text("neutral depth image coverage facade must match existing PSD coverage sampling"));
    require(near(ngDepthImageCoverageAlphaAtSource(1, 0, 3, 3, coverage, 3, 3), 64.0f / 255.0f) &&
        !ngDepthImageCoverageReliableAtSource(1, 0, 3, 3, coverage, 3, 3),
        text("neutral depth image coverage helpers should expose coverage alpha and reliability thresholds"));
    require(near(ngDepthImageCoverageAlphaAtSource(0, 0, 3, 3, ConstSlice<std::uint8_t>{}, 0, 0), 1.0f) &&
        ngDepthImageCoverageReliableAtSource(0, 0, 3, 3, ConstSlice<std::uint8_t>{}, 0, 0),
        text("neutral depth image coverage helpers should treat missing coverage as fully reliable"));
    require(near(ngDepthImageCoverageAlphaAtSource(1, 0, 3, 3, coverage.slice(0, 27), 3, 3, 3), 1.0f) &&
        ngDepthImageCoverageReliableAtSource(1, 0, 3, 3, coverage.slice(0, 27), 3, 3, 3),
        text("neutral depth image coverage helpers should preserve legacy non-alpha coverage as fully reliable"));
    Slice<std::uint8_t> uvCoverage{
        255, 255, 255, 0,   255, 255, 255, 64,
        255, 255, 255, 128, 255, 255, 255, 255,
    };
    require(near(ngDepthImageCoverageAlphaAtUvSource(0.5f, 0.5f, uvCoverage, 2, 2, 4), 111.75f / 255.0f) &&
        near(ngDepthImageCoverageAlphaAtUvSource(0.5f, 0.5f, uvCoverage.slice(0, 12), 2, 2, 3), 1.0f),
        text("neutral depth image coverage helpers should own bilinear UV alpha sampling for mesh coverage"));
    require(near(ngDepthImageCompositeAlpha(0.25f, 0.5f), 0.625f) &&
        near(ngDepthImageCompositeAlpha(-1.0f, 0.5f), 0.5f) &&
        near(ngDepthImageCompositeAlpha(0.25f, 2.0f), 1.0f),
        text("neutral depth image coverage composition should use clamped alpha-over semantics"));
    auto composedCoverage = ngDepthImageCompositeAlpha(0.0f, 0.25f);
    composedCoverage = ngDepthImageCompositeAlpha(composedCoverage, 0.5f);
    require(near(composedCoverage, 0.625f),
        text("neutral depth image coverage composition should accumulate multiple coverage sources deterministically"));

    auto depthImageOpacity = ngDepthImageSampleRgbaWithOpacitySource(pixels, 3, 3, 1, 1, 0.25f, settings);
    auto psdOpacity = ngPsdDepthSamplePixelsWithOpacity(pixels, 3, 3, 1, 1, 0.25f, psdSettings);
    require(depthImageOpacity.valid == psdOpacity.valid &&
        sameDepthSampleValue(depthImageOpacity.value, psdOpacity.value),
        text("neutral depth image opacity facade must match existing PSD opacity sampling"));
    auto frontmost = ngDepthImageFrontmostSource(Slice<DepthImageSampleResult>{
        DepthImageSampleResult{false, -1.0f}, DepthImageSampleResult{true, -0.25f}, DepthImageSampleResult{true, 0.5f}});
    require(frontmost.valid && near(frontmost.value, 0.5f),
        text("neutral depth image composition facade must preserve frontmost behavior"));
    auto sampleInputs = Slice<DepthSampleResult>{
        DepthSampleResult{false, -1.0f}, DepthSampleResult{true, 0.25f}, DepthSampleResult{true, 0.75f}};
    auto sampleFrontmost = nijigenerate::io::depthsample::ngDepthSampleFrontmost(
        std::span<const DepthSampleResult>(sampleInputs.data(), sampleInputs.size()));
    require(sampleFrontmost.valid && near(sampleFrontmost.value, 0.75f),
        text("common depth sample result should preserve frontmost behavior without PSD sample types"));

    for (const auto channel : Slice<DepthImageChannel>{DepthImageChannel::AverageRGB, DepthImageChannel::R,
            DepthImageChannel::G, DepthImageChannel::B, DepthImageChannel::Luminance}) {
        for (const auto convolution : Slice<DepthImageConvolution>{
                DepthImageConvolution::Nearest, DepthImageConvolution::Box3x3, DepthImageConvolution::Box5x5,
                DepthImageConvolution::Gaussian3x3, DepthImageConvolution::Gaussian5x5,
                DepthImageConvolution::Median3x3, DepthImageConvolution::Frontmost3x3,
                DepthImageConvolution::Backmost3x3, DepthImageConvolution::BoxCustom,
                DepthImageConvolution::GaussianCustom, DepthImageConvolution::MedianCustom,
                DepthImageConvolution::FrontmostCustom, DepthImageConvolution::BackmostCustom}) {
            settings.channel = channel;
            settings.convolution = convolution;
            settings.customRadius = 2;
            settings.invert = convolution == DepthImageConvolution::Backmost3x3 ||
                convolution == DepthImageConvolution::BackmostCustom;
            settings.depthScale = channel == DepthImageChannel::Luminance ? 0.75f : 1.0f;
            psdSettings = ngDepthImageToPsdSettings(settings);
            auto direct = ngDepthImageSampleRgbaWithCoverageSource(
                pixels, 3, 3, coverage, 3, 3, 0.75f, 1, 1, settings);
            auto legacy = ngPsdDepthSamplePixelsWithCoverage(pixels, 3, 3, coverage, 3, 3, 0.75f, 1, 1, psdSettings);
            const bool condition = direct.valid == legacy.valid && sameDepthSampleValue(direct.value, legacy.value);
            auto message = concatenateSourceStrings({
                text("neutral depth image sampling must remain deterministic against PSD sampling for channel "),
                ngDepthImageChannelNameSource(channel), text(" and convolution "),
                ngDepthImageConvolutionNameSource(convolution), text(": neutral="), floatText(direct.value),
                text(" psd="), floatText(legacy.value)});
            require(condition, message);
        }
    }

    settings = DepthImageSampleSettings{};
    settings.convolution = DepthImageConvolution::Nearest;
    settings.alphaThreshold = 0.01f;
    auto rejectedByCoverage = ngDepthImageSampleRgbaWithCoverageSource(
        pixels, 3, 3, coverage, 3, 3, 1.0f, 2, 0, settings);
    require(!rejectedByCoverage.valid,
        text("neutral depth image sampling should deterministically reject missing coverage pixels"));
    auto rejectedByOpacity = ngDepthImageSampleRgbaWithOpacitySource(
        Slice<std::uint8_t>{255, 255, 255, 255}, 1, 1, 0, 0, 0.005f, settings);
    require(!rejectedByOpacity.valid,
        text("neutral depth image sampling should deterministically apply layer opacity before alpha thresholding"));
    auto rejectedOutOfBounds = ngDepthImageSampleRgbaSource(pixels, 3, 3, -1, 1, settings);
    require(!rejectedOutOfBounds.valid,
        text("neutral depth image sampling should reject out-of-bounds document samples deterministically"));

    Slice<std::uint8_t> white4x4;
    for (int i = 0; i < 16; ++i) {
        auto pixel = Slice<std::uint8_t>{255, 255, 255, 255};
        white4x4.appendRange(pixel);
    }
    Slice<std::uint8_t> coverage2x2{
        255, 255, 255, 255, 255, 255, 255, 0,
        255, 255, 255, 0,   255, 255, 255, 255,
    };
    auto scaledCoverageAccepted = ngDepthImageSampleRgbaWithCoverageSource(
        white4x4, 4, 4, coverage2x2, 2, 2, 1.0f, 0, 0, settings);
    auto scaledCoverageRejected = ngDepthImageSampleRgbaWithCoverageSource(
        white4x4, 4, 4, coverage2x2, 2, 2, 1.0f, 3, 0, settings);
    auto scaledCoverageAcceptedBottomRight = ngDepthImageSampleRgbaWithCoverageSource(
        white4x4, 4, 4, coverage2x2, 2, 2, 1.0f, 3, 3, settings);
    require(scaledCoverageAccepted.valid && !scaledCoverageRejected.valid && scaledCoverageAcceptedBottomRight.valid,
        text("neutral depth image sampling should map scaled coverage alpha deterministically"));
    auto rejectedByCoverageOpacity = ngDepthImageSampleRgbaWithCoverageSource(
        white4x4, 4, 4, coverage2x2, 2, 2, 0.005f, 0, 0, settings);
    require(!rejectedByCoverageOpacity.valid,
        text("neutral depth image sampling should apply coverage layer opacity before alpha thresholding"));
    bool threwBadRgba = false;
    try {
        ngDepthImageSampleRgbaSource(Slice<std::uint8_t>{255, 255, 255}, 1, 1, 0, 0, settings);
    } catch (const ThrowableException&) {
        threwBadRgba = true;
    }
    require(threwBadRgba,
        text("neutral depth image sampling should reject undersized RGBA buffers deterministically"));
}
} // namespace nijigenerate_tests::regression::detail
