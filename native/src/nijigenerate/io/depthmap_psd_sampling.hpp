#pragma once
#include "depthmap_psd_public_sampling.hpp"
#include "depthmap_psd_internal_values.hpp"
#include "depthimage.hpp"
#include <common/shared_array.hpp>
#include <array>
#include <span>
namespace nijigenerate::io::depthmap_psd::detail {
using Vec2 = nicxlive::core::math::Vec2;
using depthimage::DepthDrawSplitLayer;
using depthsample::DepthSampleAggregate;
float pixelDepth01(const compat::Slice<std::uint8_t> data, size_t index, PsdDepthImportSettings& settings);
float pixelDepth(const compat::Slice<std::uint8_t> data, size_t index, PsdDepthImportSettings& settings);
float layerToSourceX(DepthLayerImage& layer, float documentX);
float layerToSourceY(DepthLayerImage& layer, float documentY);
float sampledDepth01(float value, PsdDepthImportSettings& settings);
float layerOpacity01(std::uint8_t opacity);
float effectiveAlpha(std::span<const std::uint8_t> rgba, size_t index, float opacity);
float coverageAlpha(DepthLayerImage& layer, int x, int y);
bool coverageReliable(DepthLayerImage& layer, int x, int y);
float coverageCacheValue(compat::Slice<float>& cache, DepthLayerImage& layer, int x, int y);
float coverageSourcesAlpha(DepthLayerImage& layer, int x, int y, bool includeOpacity);
float coveragePixelAlpha(DepthLayerImage& layer, int x, int y);
bool barycentric(Vec2 p, Vec2 a, Vec2 b, Vec2 c, float& w0, float& w1, float& w2);
float sampleCoverageSourceAlpha(CoverageSource& source, Vec2 local);
float sampleCoverageSourceAlphaAtUv(CoverageSource& source, Vec2 uv);
void sampleCoverageSourceRgbaAtUv(CoverageSource& source, Vec2 uv, std::array<std::uint8_t, 4>& rgba);
bool sampleCoverageSourceRgba(CoverageSource& source, Vec2 local, std::array<std::uint8_t, 4>& rgba);
ColorLayerPreviewRgba buildColorLayerPreviewRgba(DepthLayerImage& image);
void cropDepthLayerImageToRect(DepthLayerImage& image, int left, int top, int width, int height);
bool tightenDepthLayerImageToCoverageBounds(DepthLayerImage& image);
bool setDepthLayerImageToCoverageDocumentBounds(DepthLayerImage& image);
Vec2 worldToLayerPixel(DepthLayerImage& layer, Vec2 world);
void buildCoverageCache(DepthLayerImage& layer);
void maskDepthToCoverage(DepthLayerImage& layer, PsdDepthImportSettings& settings);
void applyDepthDrawLayerDepthCleanup(DepthLayerImage& layer, PsdDepthImportSettings& settings);
DepthDrawSplitLayer depthDrawSplitLayerFromCompositeSource(PsdDepthCompositeSourceLayer sourceLayer);
compat::Slice<std::uint8_t> depthDrawMaskFromDepthLayerImage(DepthLayerImage& image);
DepthDrawSplitLayer depthDrawSplitLayerFromDepthLayerImage(DepthLayerImage& image);
void setLayerDepthFromDepthDrawPixels(DepthLayerImage& image, std::span<const std::uint8_t> depthPixels,
    std::span<const std::uint8_t> alphaMask = {});
bool hasVisibleDepthDrawPixels(std::span<const std::uint8_t> depthPixels,
    std::span<const std::uint8_t> alphaMask = {});
std::ptrdiff_t bestMatchingColorSourceLayer(PsdDepthCompositeSourceLayer depthLayer,
    compat::Slice<PsdDepthCompositeSourceLayer> colorLayers,
    nicxlive::core::common::SharedArray<bool> reserved = {});
void seedLayerDepthFromDepthDrawSplit(
    DepthLayerImage& image,
    DepthDrawSplitLayer splitLayer,
    int layerIndex,
    int documentWidth,
    int documentHeight,
    compat::ConstSlice<std::uint8_t> stableDepthPixels,
    compat::ConstSlice<int> visibleLayerMap,
    compat::ConstSlice<DepthDrawSplitLayer> splitLayers,
    bool repairContourBand
);
float sampleCoverageAlphaAtUv(DepthLayerImage& layer, Vec2 uv);
float effectiveAlpha(DepthLayerImage& layer, size_t index, int x, int y);
float surfaceAlpha(DepthLayerImage& layer, size_t index, int x, int y);
std::uint8_t effectiveAlphaByte(DepthLayerImage& layer, size_t index, int x, int y);
bool acceptsDepthPixel(DepthLayerImage& layer, size_t index, int x, int y, PsdDepthImportSettings& settings);
void attachCompositeSourceCoverage(
    DepthLayerImage& image,
    PsdDepthCompositeSourceLayer colorLayer
);
PsdClippingBaseState psdClippingBaseState(DepthLayerImage& image, bool visible, float opacity);
void applyPsdClippingBaseAlpha(DepthLayerImage& image, const PsdClippingBaseState& base);
compat::Slice<std::uint8_t> buildDepthMaskPreview(DepthLayerImage& layer, PsdDepthImportSettings& settings);
compat::Slice<std::uint8_t> buildDepthMaskPreviewFromSurface(
    DepthLayerImage& layer,
    PsdDepthImportSettings& settings,
    compat::Slice<std::uint8_t> surfaceRgba
);
compat::Slice<std::uint8_t> buildDepthMaskPreviewFromSurface(
    DepthLayerImage& layer,
    PsdDepthImportSettings& settings,
    compat::ConstSlice<std::uint8_t> surfaceRgba
);
compat::Slice<std::uint8_t> buildDepthMaskPreviewFromSurface(
    DepthLayerImage& layer,
    PsdDepthImportSettings& settings,
    std::span<const std::uint8_t> surfaceRgba
);
compat::Slice<std::uint8_t> sampleDepthToSurfaceLayer(
    std::span<const std::uint8_t> depthRgba,
    int depthWidth,
    int depthHeight,
    int layerLeft,
    int layerTop,
    int layerWidth,
    int layerHeight
);
compat::Slice<std::uint8_t> sampleDepthToSurfaceLayerFlippedY(
        std::span<const std::uint8_t> depthRgba,
        int depthWidth,
        int depthHeight,
        int layerLeft,
    int layerTop,
    int layerWidth,
    int layerHeight
);
size_t depthSurfaceOverlap(
    std::span<const std::uint8_t> depthRgba,
    std::span<const std::uint8_t> surfaceRgba
);
void compositeFlatDepthByCoverageSources(
    DepthLayerImage& image,
    std::span<const std::uint8_t> depthRgba,
    int depthWidth,
    int depthHeight
);
PsdDepthLayerDepthStats computeLayerDepthStats(DepthLayerImage& layer, PsdDepthImportSettings& settings);
DepthSample samplePixel(DepthLayerImage& layer, int x, int y, PsdDepthImportSettings& settings);
DepthSample aggregateToDepthSample(DepthSampleAggregate aggregate);
DepthSample sampleLayer(DepthLayerImage& layer, float x, float y, PsdDepthImportSettings& settings);
}
namespace nijigenerate::io::depthmap_psd {
namespace detail {
void finalizeGridResult(PsdDepthGridResult& result, GridAccum& accum, PsdDepthImportSettings& settings);
void buildCompositePreview(
    PsdDepthGridResult& result,
    compat::Slice<DepthLayerImage> layers,
    std::shared_ptr<nicxlive::core::nodes::Deformable> grid,
    PsdDepthImportSettings& settings
);
}
}
