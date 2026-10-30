#pragma once
#include "map.hpp"
#include <nijigenerate/viewport/depth/draw/pngexport.hpp>
namespace nijigenerate::commands::depth::map {
io::depthimage::DepthImageConvolution ngPsdDepthConvolutionToDepthImage(
    io::depthmap_psd::PsdDepthConvolution convolution);
std::shared_ptr<viewport::depth::draw::DepthDrawSession> ngPsdDepthComposedSourceToDepthDrawSession(
    io::depthmap_psd::PsdDepthComposedSource source);
std::shared_ptr<viewport::depth::draw::DepthDrawSession> ngPsdDepthImportResultToDepthDrawSession(
    io::depthmap_psd::PsdDepthImportResult imported);
viewport::depth::draw::DepthDrawPngExportResult ngExportPsdDepthComposedSourcePng(
    io::depthmap_psd::PsdDepthComposedSource source, compat::RetainedString outputDir,
    compat::RetainedString manifestPath);
namespace detail {
void updatePsdDepthGridRange(io::depthmap_psd::PsdDepthGridResult& gridResult);
}
}
