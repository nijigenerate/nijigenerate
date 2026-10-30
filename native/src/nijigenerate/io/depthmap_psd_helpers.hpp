#pragma once
#include "depthmap_psd_internal_values.hpp"
#include "depthmap_psd_enum_functions.hpp"
#include "depthsample.hpp"
#include <common/shared_array.hpp>
#include <span>
namespace nijigenerate::io::depthmap_psd::detail {
depthsample::DepthSampleChannel sampleChannel(PsdDepthChannel channel);
depthsample::DepthSampleConvolution sampleConvolution(PsdDepthConvolution convolution);
float clamp01(float value);
compat::Slice<std::uint8_t> cropRgbaRect(std::span<const std::uint8_t> rgba,
    int sourceWidth, int sourceHeight, int left, int top, int width, int height);
compat::Slice<std::uint8_t> normalizeFlatDepthRgba(std::span<const std::uint8_t> rgba,
    int width, int height, int channels = 4);
compat::Slice<std::uint8_t> flipRgbaY(std::span<const std::uint8_t> rgba, int width, int height);
float compositeSourceLayerMatchScore(PsdDepthCompositeSourceLayer color, PsdDepthCompositeSourceLayer depth);
std::ptrdiff_t bestMatchingDepthSourceLayer(PsdDepthCompositeSourceLayer color,
    compat::Slice<PsdDepthCompositeSourceLayer> depths,
    nicxlive::core::common::SharedArray<bool> reserved = {});
}
