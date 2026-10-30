#pragma once

#include "depthsample.hpp"
#include "depthmap_psd.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nijigenerate::io::depthimage {

using DepthImageChannel = depthsample::DepthSampleChannel;
using DepthImageConvolution = depthsample::DepthSampleConvolution;
using DepthImageSampleResult = depthsample::DepthSampleResult;

inline constexpr int DepthImageMinCustomRadius = 1;
inline constexpr int DepthImageMaxCustomRadius = 64;

int ngNormalizeDepthImageCustomRadius(int radius) noexcept;

struct DepthImageSampleSettings {
    bool invert = false;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float depthScale = 1.0f;
    float alphaThreshold = 0.01f;
    int customRadius = 3;
    DepthImageConvolution convolution = DepthImageConvolution::Gaussian3x3;
    DepthImageChannel channel = DepthImageChannel::AverageRGB;
};

depthmap_psd::PsdDepthImportSettings ngDepthImageToPsdSettings(DepthImageSampleSettings settings);

std::string ngDepthImageChannelName(DepthImageChannel value);
compat::RetainedString ngDepthImageChannelNameSource(DepthImageChannel value);
DepthImageChannel ngDepthImageChannelFromString(std::string_view value);
DepthImageChannel ngDepthImageChannelFromStringSource(compat::RetainedString value);
std::string ngDepthImageConvolutionName(DepthImageConvolution value);
compat::RetainedString ngDepthImageConvolutionNameSource(DepthImageConvolution value);
DepthImageConvolution ngDepthImageConvolutionFromString(std::string_view value);
DepthImageConvolution ngDepthImageConvolutionFromStringSource(compat::RetainedString value);
DepthImageSampleResult ngDepthImageFrontmost(std::span<const DepthImageSampleResult> samples);
DepthImageSampleResult ngDepthImageFrontmostSource(compat::Slice<DepthImageSampleResult> samples);
std::vector<std::uint8_t> ngDepthDrawDepthPixelsFromRgbaRed(std::span<const std::uint8_t> rgba);
compat::Slice<std::uint8_t> ngDepthDrawDepthPixelsFromRgbaRedSource(
    compat::ConstSlice<std::uint8_t> rgba);

float ngDepthImageCoverageAlphaAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight);
float ngDepthImageCoverageAlphaAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight, int coverageChannels);
bool ngDepthImageCoverageReliableAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight);
bool ngDepthImageCoverageReliableAt(int x, int y, int width, int height,
    std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight, int coverageChannels);
float ngDepthImageCoverageAlphaAtUv(float u, float v, std::span<const std::uint8_t> coverageRgba,
    int coverageWidth, int coverageHeight, int coverageChannels);
float ngDepthImageCoverageAlphaAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight);
float ngDepthImageCoverageAlphaAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight, int coverageChannels);
bool ngDepthImageCoverageReliableAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight);
bool ngDepthImageCoverageReliableAtSource(int x, int y, int width, int height,
    compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight, int coverageChannels);
float ngDepthImageCoverageAlphaAtUvSource(float u, float v, compat::ConstSlice<std::uint8_t> coverageRgba,
    int coverageWidth, int coverageHeight, int coverageChannels);
float ngDepthImageCompositeAlpha(float dst, float alpha);

DepthImageSampleResult ngDepthImageSampleRgba(std::span<const std::uint8_t> rgba,
    int width, int height, float x, float y, DepthImageSampleSettings settings);
DepthImageSampleResult ngDepthImageSampleRgbaWithOpacity(std::span<const std::uint8_t> rgba,
    int width, int height, float x, float y, float opacity, DepthImageSampleSettings settings);
DepthImageSampleResult ngDepthImageSampleRgbaWithCoverage(std::span<const std::uint8_t> rgba,
    int width, int height, std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float coverageOpacity, float x, float y, DepthImageSampleSettings settings);
DepthImageSampleResult ngDepthImageSampleRgbaWithOpacityAndCoverage(std::span<const std::uint8_t> rgba,
    int width, int height, std::span<const std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float layerOpacity, float coverageOpacity, float x, float y, DepthImageSampleSettings settings);
DepthImageSampleResult ngDepthImageSampleRgbaSource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, float x, float y, DepthImageSampleSettings settings);
DepthImageSampleResult ngDepthImageSampleRgbaWithOpacitySource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, float x, float y, float opacity, DepthImageSampleSettings settings);
DepthImageSampleResult ngDepthImageSampleRgbaWithCoverageSource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float coverageOpacity, float x, float y, DepthImageSampleSettings settings);
DepthImageSampleResult ngDepthImageSampleRgbaWithOpacityAndCoverageSource(compat::ConstSlice<std::uint8_t> rgba,
    int width, int height, compat::ConstSlice<std::uint8_t> coverageRgba, int coverageWidth, int coverageHeight,
    float layerOpacity, float coverageOpacity, float x, float y, DepthImageSampleSettings settings);

// Struct copies retain D array backing storage instead of duplicating pixels.
struct DepthDrawGlobalDepthScaleResult {
    compat::Slice<std::uint8_t> pixels;
    double centroid = std::numeric_limits<double>::quiet_NaN();
    std::size_t count = 0;
};

struct DepthDrawDepthOverrideMasks {
    compat::Slice<std::uint8_t> colorMask;
    compat::Slice<std::uint8_t> strictSurfaceMask;
    compat::Slice<std::uint8_t> surfaceMask;
    compat::Slice<std::uint8_t> renderDepthMask;
};

struct DepthDrawAlphaDepthFocusedRule {
    int layerIndex = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int lift = 0;
    int radius = 0;
};

struct DepthDrawAlphaDepthGapDetection {
    compat::Slice<std::uint8_t> mask;
    std::size_t zero = 0;
    std::size_t depression = 0;
    std::size_t cliff = 0;
    std::size_t focusedAdded = 0;
    std::size_t total = 0;
};

struct DepthDrawAlphaDepthGapFillResult {
    compat::Slice<std::uint8_t> depth;
    std::size_t filled = 0;
    std::size_t remaining = 0;
};

struct DepthDrawDepthInpaintResult {
    compat::Slice<std::uint8_t> pixels;
    compat::Slice<std::uint8_t> filledMask;
};

struct DepthDrawPruneLayer {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
};

struct DepthDrawDepthPruneResult {
    compat::Slice<std::uint8_t> pixels;
    compat::Slice<std::uint8_t> debugState;
    compat::Slice<std::uint8_t> debugScore;
};

struct DepthDrawSplitLayer {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    compat::Slice<std::uint8_t> maskPixels;
    compat::Slice<std::uint8_t> alphaMask;
};

inline constexpr std::uint64_t DepthDrawMaxHistogramIntegralBytes = 320ULL * 1024ULL * 1024ULL;

std::vector<std::uint8_t> ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba(std::span<const std::uint8_t> rgba);
compat::Slice<std::uint8_t> ngDepthDrawDecodeGrayscaleDepthPixelsFromRgbaSource(
    compat::ConstSlice<std::uint8_t> rgba);
compat::Slice<std::uint8_t> ngDepthDrawDecodeDepthPixelsFromRgbaSource(
    compat::ConstSlice<std::uint8_t> rgba, DepthImageChannel channel);
compat::Slice<std::uint8_t> ngDepthDrawAlphaMaskFromRgbaSource(
    compat::ConstSlice<std::uint8_t> rgba, std::uint8_t minAlpha = 1);
std::vector<std::uint8_t> ngDepthDrawDecodeDepthPixelsFromRgba(
    std::span<const std::uint8_t> rgba, DepthImageChannel channel);
std::vector<std::uint8_t> ngDepthDrawAlphaMaskFromRgba(
    std::span<const std::uint8_t> rgba, std::uint8_t minAlpha = 1);
std::vector<std::uint8_t> ngDepthDrawMaskFromDepthPixels(std::span<const std::uint8_t> depthPixels);
compat::Slice<std::uint8_t> ngDepthDrawMaskFromDepthPixelsSource(compat::ConstSlice<std::uint8_t> depthPixels);
std::vector<std::uint8_t> ngDepthDrawApplyBinaryMaskToDepthPixels(
    std::span<const std::uint8_t> depthPixels, std::span<const std::uint8_t> mask);
compat::Slice<std::uint8_t> ngDepthDrawApplyBinaryMaskToDepthPixelsSource(
    compat::ConstSlice<std::uint8_t> depthPixels, compat::ConstSlice<std::uint8_t> mask);
std::vector<std::uint8_t> ngDepthDrawSampleDepthLayerToColorLayer(std::span<const std::uint8_t> depthPixels,
    int depthWidth, int depthHeight, int depthLeft, int depthTop, std::span<const std::uint8_t> depthAlphaMask,
    int colorWidth, int colorHeight, int colorLeft, int colorTop);
compat::Slice<std::uint8_t> ngDepthDrawSampleDepthLayerToColorLayerSource(
    compat::ConstSlice<std::uint8_t> depthPixels, int depthWidth, int depthHeight,
    int depthLeft, int depthTop, compat::ConstSlice<std::uint8_t> depthAlphaMask,
    int colorWidth, int colorHeight, int colorLeft, int colorTop);
std::uint8_t ngDepthDrawScaleDepthValueAroundCenter(std::uint8_t depth, double scale, double center);
DepthDrawGlobalDepthScaleResult ngDepthDrawApplyGlobalDepthScale(
    compat::ConstSlice<std::uint8_t> pixels, double scale, compat::ConstSlice<std::uint8_t> maskPixels = {});
DepthDrawGlobalDepthScaleResult ngDepthDrawApplyGlobalDepthScale(
    std::span<const std::uint8_t> pixels, double scale, std::span<const std::uint8_t> maskPixels = {});
template<class Pixels, class Mask = compat::ConstSlice<std::uint8_t>>
requires ((std::same_as<Pixels, compat::Slice<std::uint8_t>> ||
           std::same_as<Pixels, compat::ConstSlice<std::uint8_t>>) &&
          (std::same_as<Mask, compat::Slice<std::uint8_t>> ||
           std::same_as<Mask, compat::ConstSlice<std::uint8_t>>))
DepthDrawGlobalDepthScaleResult ngDepthDrawApplyGlobalDepthScale(
        Pixels pixels, double scale, Mask maskPixels = {}) {
    return ngDepthDrawApplyGlobalDepthScale(
        std::span<const std::uint8_t>{pixels.data(), pixels.size()}, scale,
        std::span<const std::uint8_t>{maskPixels.data(), maskPixels.size()});
}
std::vector<std::uint8_t> ngDepthDrawApplyPsdMaskToAlpha(std::span<const std::uint8_t> alpha,
    int width, int height, int layerLeft, int layerTop, std::span<const std::uint8_t> mask,
    int maskWidth, int maskHeight, int maskLeft, int maskTop, bool positionRelativeToLayer,
    std::uint8_t defaultMask = 255);
compat::Slice<std::uint8_t> ngDepthDrawApplyPsdMaskToAlphaSource(compat::ConstSlice<std::uint8_t> alpha,
    int width, int height, int layerLeft, int layerTop, compat::ConstSlice<std::uint8_t> mask,
    int maskWidth, int maskHeight, int maskLeft, int maskTop, bool positionRelativeToLayer,
    std::uint8_t defaultMask = 255);
std::vector<std::uint8_t> ngDepthDrawScaleAlphaMask(std::span<const std::uint8_t> sourceMask,
    int sourceWidth, int sourceHeight, int width, int height);
compat::Slice<std::uint8_t> ngDepthDrawScaleAlphaMaskSource(compat::ConstSlice<std::uint8_t> sourceMask,
    int sourceWidth, int sourceHeight, int width, int height);
std::vector<std::uint8_t> ngDepthDrawMaskFromAlphaGreaterThan(
    std::span<const std::uint8_t> alpha, std::uint8_t minAlpha);
compat::Slice<std::uint8_t> ngDepthDrawMaskFromAlphaGreaterThanSource(
    compat::ConstSlice<std::uint8_t> alpha, std::uint8_t minAlpha);
DepthDrawDepthOverrideMasks ngDepthDrawDepthOverrideMasks(
    std::span<const std::uint8_t> colorAlpha, std::span<const std::uint8_t> depthPixels);
DepthDrawDepthOverrideMasks ngDepthDrawDepthOverrideMasksSource(
    compat::ConstSlice<std::uint8_t> colorAlpha, compat::ConstSlice<std::uint8_t> depthPixels);
std::vector<std::uint8_t> ngDepthDrawPsdExportDepthPixels(int sourceWidth, int sourceHeight,
    int sourceLeft, int sourceTop, std::span<const std::uint8_t> sourceAlpha,
    int preparedLeft, int preparedTop, int preparedWidth, int preparedHeight,
    std::span<const std::uint8_t> exportDepthPixels, std::span<const std::uint8_t> sourceDepthPixels,
    std::span<const std::uint8_t> coverageMask);
compat::Slice<std::uint8_t> ngDepthDrawPsdExportDepthPixelsSource(int sourceWidth, int sourceHeight,
    int sourceLeft, int sourceTop, compat::ConstSlice<std::uint8_t> sourceAlpha,
    int preparedLeft, int preparedTop, int preparedWidth, int preparedHeight,
    compat::ConstSlice<std::uint8_t> exportDepthPixels, compat::ConstSlice<std::uint8_t> sourceDepthPixels,
    compat::ConstSlice<std::uint8_t> coverageMask);
std::vector<float> ngDepthDrawSmoothGridDepthValues(std::span<const float> sourceDepths,
    std::span<const std::uint8_t> vertexValid, std::span<const int> vertexGroup,
    int cols, int rows, float depthThreshold = 18.0f, float depthScale = 1.0f);
compat::Slice<float> ngDepthDrawSmoothGridDepthValuesSource(compat::ConstSlice<float> sourceDepths,
    compat::ConstSlice<std::uint8_t> vertexValid, compat::ConstSlice<int> vertexGroup,
    int cols, int rows, float depthThreshold = 18.0f, float depthScale = 1.0f);
DepthDrawAlphaDepthGapDetection ngDepthDrawDetectAlphaDepthGaps(std::span<const std::uint8_t> depth,
    std::span<const std::uint8_t> alphaMask, int width, int height, int layerIndex = 0,
    std::span<const DepthDrawAlphaDepthFocusedRule> focusedRules = {});
DepthDrawAlphaDepthGapDetection ngDepthDrawDetectAlphaDepthGapsSource(
    compat::ConstSlice<std::uint8_t> depth, compat::ConstSlice<std::uint8_t> alphaMask,
    int width, int height, int layerIndex = 0,
    compat::ConstSlice<DepthDrawAlphaDepthFocusedRule> focusedRules = {});
DepthDrawAlphaDepthGapFillResult ngDepthDrawMedianFillDepth(std::span<const std::uint8_t> sourceDepth,
    std::span<const std::uint8_t> alphaMask, std::span<const std::uint8_t> fillMask, int width, int height);
std::vector<std::uint8_t> ngDepthDrawBuildLayerContourBandMask(
    std::span<const std::uint8_t> maskPixels, int width, int height, int thickness);
compat::Slice<std::uint8_t> ngDepthDrawBuildLayerContourBandMaskSource(
    compat::ConstSlice<std::uint8_t> maskPixels, int width, int height, int thickness);
std::vector<std::uint8_t> ngDepthDrawErodePositiveDepthMask(
    std::span<const std::uint8_t> depthPixels, int width, int height, int thickness);
compat::Slice<std::uint8_t> ngDepthDrawErodePositiveDepthMaskSource(
    compat::ConstSlice<std::uint8_t> depthPixels, int width, int height, int thickness);
DepthDrawDepthInpaintResult ngDepthDrawInpaintMaskedLayerDepth(std::span<const std::uint8_t> sourceDepthPixels,
    std::span<const std::uint8_t> maskPixels, int width, int height);
DepthDrawDepthInpaintResult ngDepthDrawInpaintMaskedLayerDepthSource(
    compat::ConstSlice<std::uint8_t> sourceDepthPixels, compat::ConstSlice<std::uint8_t> maskPixels,
    int width, int height);
std::vector<std::uint8_t> ngDepthDrawSmoothMaskedPositiveDepth(std::span<const std::uint8_t> sourceDepthPixels,
    std::span<const std::uint8_t> maskPixels, int width, int height);
compat::Slice<std::uint8_t> ngDepthDrawSmoothMaskedPositiveDepthSource(
    compat::ConstSlice<std::uint8_t> sourceDepthPixels, compat::ConstSlice<std::uint8_t> maskPixels,
    int width, int height);
DepthDrawDepthPruneResult ngDepthDrawPruneForeignDepthSeeds(std::span<const std::uint8_t> sourceDepthPixels,
    DepthDrawPruneLayer layer, int layerIndex, int imageWidth, int imageHeight,
    std::span<const std::uint8_t> stableDepthPixels, std::span<const int> visibleLayerMap,
    std::span<const std::uint8_t> maskPixels, double threshold);
template<class Source, class Stable, class Visible, class Mask>
    requires ((std::same_as<Source, compat::Slice<std::uint8_t>> ||
               std::same_as<Source, compat::ConstSlice<std::uint8_t>>) &&
              (std::same_as<Stable, compat::Slice<std::uint8_t>> ||
               std::same_as<Stable, compat::ConstSlice<std::uint8_t>>) &&
              (std::same_as<Visible, compat::Slice<int>> ||
               std::same_as<Visible, compat::ConstSlice<int>>) &&
              (std::same_as<Mask, compat::Slice<std::uint8_t>> ||
               std::same_as<Mask, compat::ConstSlice<std::uint8_t>>))
DepthDrawDepthPruneResult ngDepthDrawPruneForeignDepthSeeds(Source sourceDepthPixels,
    DepthDrawPruneLayer layer, int layerIndex, int imageWidth, int imageHeight,
    Stable stableDepthPixels, Visible visibleLayerMap, Mask maskPixels, double threshold) {
    // Keep all four actual array descriptors alive through the synchronous calculation.
    return ngDepthDrawPruneForeignDepthSeeds(
        std::span<const std::uint8_t>{sourceDepthPixels.data(), sourceDepthPixels.size()},
        layer, layerIndex, imageWidth, imageHeight,
        std::span<const std::uint8_t>{stableDepthPixels.data(), stableDepthPixels.size()},
        std::span<const int>{visibleLayerMap.data(), visibleLayerMap.size()},
        std::span<const std::uint8_t>{maskPixels.data(), maskPixels.size()}, threshold);
}
std::vector<int> ngDepthDrawBuildVisibleLayerMap(
    int imageWidth, int imageHeight, std::span<const DepthDrawSplitLayer> layers);
compat::Slice<int> ngDepthDrawBuildVisibleLayerMapSource(
    int imageWidth, int imageHeight, compat::ConstSlice<DepthDrawSplitLayer> layers);
std::vector<std::uint8_t> ngDepthDrawSeedLayerDepthPixels(DepthDrawSplitLayer layer,
    int layerIndex, int imageWidth, int imageHeight, std::span<const std::uint8_t> stableDepthPixels,
    std::span<const int> visibleLayerMap, std::span<const std::uint8_t> maskPixels,
    std::span<const DepthDrawSplitLayer> layers, std::span<const std::uint8_t> contourBandMask = {},
    int upperMaskRadius = 2);
compat::Slice<std::uint8_t> ngDepthDrawSeedLayerDepthPixelsSource(DepthDrawSplitLayer layer,
    int layerIndex, int imageWidth, int imageHeight, compat::ConstSlice<std::uint8_t> stableDepthPixels,
    compat::ConstSlice<int> visibleLayerMap, compat::ConstSlice<std::uint8_t> maskPixels,
    compat::ConstSlice<DepthDrawSplitLayer> layers, compat::ConstSlice<std::uint8_t> contourBandMask = {},
    int upperMaskRadius = 2);
bool ngDepthDrawHasUpperLayerMaskNearby(std::span<const DepthDrawSplitLayer> layers,
    int layerIndex, int globalX, int globalY, int radius);
bool ngDepthDrawHasUpperLayerMaskNearbySource(compat::ConstSlice<DepthDrawSplitLayer> layers,
    int layerIndex, int globalX, int globalY, int radius);
std::vector<std::uint8_t> ngDepthDrawCreateMaskedGridDepthPixels(int width, int height,
    std::span<const std::uint8_t> sourcePixels, std::span<const std::uint8_t> maskPixels,
    std::string_view specMode, int gridX, int gridY, int kernelSize, std::string_view interpMode);
compat::Slice<std::uint8_t> ngDepthDrawCreateMaskedGridDepthPixelsSource(int width, int height,
    compat::ConstSlice<std::uint8_t> sourcePixels, compat::ConstSlice<std::uint8_t> maskPixels,
    compat::RetainedString specMode, int gridX, int gridY, int kernelSize, compat::RetainedString interpMode);
bool ngDepthDrawHistogramIntegralDimensionsSupported(int width, int height);

}

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_IMAGE_VALUE_ORIGIN(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::io::depthimage::Type> { \
    inline static constexpr bool isNativeObject=false; \
    inline static constexpr char qualifiedArgument[]="(nijigenerate.io.depthimage." #Type ")"; \
    inline static constexpr char unqualifiedArgument[]="(" #Type ")"; \
    inline static constexpr char qualifiedElement[]="nijigenerate.io.depthimage." #Type; \
    inline static constexpr char unqualifiedElement[]=#Type; \
};
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthImageSampleSettings)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawGlobalDepthScaleResult)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawDepthOverrideMasks)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawAlphaDepthFocusedRule)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawAlphaDepthGapDetection)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawAlphaDepthGapFillResult)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawDepthInpaintResult)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawPruneLayer)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawDepthPruneResult)
NG_DEPTH_IMAGE_VALUE_ORIGIN(DepthDrawSplitLayer)
#undef NG_DEPTH_IMAGE_VALUE_ORIGIN
} // namespace nicxlive::core::d_runtime
