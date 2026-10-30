#pragma once

#include "depthmap_psd_values.hpp"
#include <nijigenerate/compat/const_slice.hpp>
#include <span>

namespace nijigenerate::io::depthmap_psd {

PsdDepthSampleResult ngPsdDepthSamplePixels(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixels(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixels(
    std::span<const std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithOpacity(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithOpacity(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithOpacity(
    std::span<const std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    compat::Slice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    compat::ConstSlice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    compat::ConstSlice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    compat::Slice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    std::span<const std::uint8_t> rgba,
    int width,
    int height,
    std::span<const std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
);
PsdDepthSampleResult ngPsdDepthFrontmost(compat::Slice<PsdDepthSampleResult> samples);

#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
compat::Slice<std::uint8_t> ngPsdDepthAttachCompositeSourceCoverageForRegression(
    int depthLeft,
    int depthTop,
    int depthWidth,
    int depthHeight,
    PsdDepthCompositeSourceLayer colorLayer
);
compat::Slice<std::ptrdiff_t> ngPsdDepthPairCompositeSourceLayersForRegression(
    compat::Slice<PsdDepthCompositeSourceLayer> colorLayers,
    compat::Slice<PsdDepthCompositeSourceLayer> depthLayers
);
compat::Slice<std::ptrdiff_t> ngPsdDepthPairCoverageSourceLayersForRegression(
    compat::Slice<PsdDepthCompositeSourceLayer> depthLayers,
    compat::Slice<PsdDepthCompositeSourceLayer> colorLayers
);
#endif

} // namespace nijigenerate::io::depthmap_psd
