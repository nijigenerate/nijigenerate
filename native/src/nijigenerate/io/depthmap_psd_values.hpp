// Complete records from the independent depthmap_psd.d original.
// Arrays retain shared backing; floating D defaults are NaN.
#pragma once
#include "depthmap_psd_types.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <common/shared_array.hpp>
#include <memory>
namespace nicxlive::core::nodes { class Deformable; }
namespace nijigenerate::io::depthmap_psd {
struct PsdDepthLayerMapping {
    compat::RetainedString layerPath{};
    compat::RetainedString layerName{};
    compat::RetainedString matchedNodeName{};
    compat::RetainedString targetGridName{};
    std::uint64_t matchedNodeUuid{};
    std::uint64_t targetGridUuid{};
    bool matched{};
    bool ambiguous{};
    bool ignored{};
    bool manual{};
    compat::RetainedString status{};
};
struct PsdDepthGridLayerMask {
    compat::RetainedString layerPath{};
    compat::RetainedString layerName{};
    std::size_t sampledVertices{};
    std::size_t selectedVertices{};
};
struct PsdDepthLayerDepthStats {
    std::size_t maskedPixels{};
    std::size_t zeroPixels{};
    float minDepth01{std::numeric_limits<float>::quiet_NaN()};
    float maxDepth01{std::numeric_limits<float>::quiet_NaN()};
    float rangeDepth01{std::numeric_limits<float>::quiet_NaN()};
    float adjacentDelta01{std::numeric_limits<float>::quiet_NaN()};
    bool hasDepth{};
};
struct PsdDepthCompositionDiagnostic {
    compat::RetainedString type{};
    compat::RetainedString message{};
    compat::RetainedString layerPath{};
    compat::RetainedString layerName{};
    std::size_t count{};
};
struct PsdDepthCompositeSourceLayer {
    compat::RetainedString id{};
    compat::RetainedString name{};
    int left{};
    int top{};
    int width{};
    int height{};
    bool visible{true};
    bool hasPixels{};
    float opacity{1.0f};
    std::size_t maskedPixels{};
    std::uint64_t targetGridUuid{};
    compat::RetainedString targetGridName{};
    compat::Slice<std::uint8_t> rgba{};
    compat::Slice<std::uint8_t> maskRgba{};
};
struct PsdDepthCompositeSource {
    PsdDepthCompositeSourceKind kind{};
    compat::RetainedString kindName{};
    compat::RetainedString sourcePath{};
    int width{};
    int height{};
    compat::Slice<PsdDepthCompositeSourceLayer> layers{};
};
struct PsdDepthComposedLayer {
    compat::RetainedString id{};
    compat::RetainedString layerPath{};
    compat::RetainedString layerName{};
    compat::RetainedString colorLayerPath{};
    compat::RetainedString colorLayerName{};
    compat::RetainedString sourcePath{};
    compat::RetainedString depthLayerName{};
    int left{};
    int top{};
    int width{};
    int height{};
    bool visible{true};
    bool enabled{true};
    bool depthEnabled{true};
    bool invert{};
    float depthOffset{0.0f};
    float depthScale{1.0f};
    float backDepth{-1.0f};
    float frontDepth{1.0f};
    float sourceDepthScale{1.0f};
    float alphaThreshold{0.01f};
    int customRadius{3};
    PsdDepthConvolution convolution{PsdDepthConvolution::Gaussian3x3};
    bool outlierPruneEnabled{};
    bool puppetFitEnabled{true};
    compat::RetainedString puppetBindingOverride{};
    std::uint64_t targetGridUuid{};
    compat::RetainedString targetGridName{};
    PsdDepthLayerDepthStats depthStats{};
    compat::Slice<std::uint8_t> colorRgba{};
    compat::Slice<std::uint8_t> depthRgba{};
    compat::Slice<std::uint8_t> maskRgba{};
    compat::Slice<std::uint8_t> coverageMaskRgba{};
};
struct PsdDepthComposedSource {
    PsdDepthCompositionMode mode{};
    compat::RetainedString modeName{};
    int width{};
    int height{};
    float globalDepthScale{1.0f};
    float globalDepthCentroid{0.0f};
    PsdDepthColorComposite colorSource{};
    PsdDepthDepthComposite depthSource{};
    compat::Slice<PsdDepthComposedLayer> layers{};
    compat::Slice<PsdDepthCompositionDiagnostic> diagnostics{};
};
struct PsdDepthGridResult {
    std::shared_ptr<nicxlive::core::nodes::Deformable> grid{};
    compat::Slice<float> depths{};
    compat::Slice<float> baseDepths{};
    compat::Slice<compat::RetainedString> winnerLayerPaths{};
    compat::Slice<PsdDepthGridLayerMask> layerMasks{};
    std::size_t coverageSources{};
    int documentWidth{};
    int documentHeight{};
    int previewLeft{};
    int previewTop{};
    int previewWidth{};
    int previewHeight{};
    compat::Slice<std::uint8_t> rawCompositePreviewRgba{};
    compat::Slice<std::uint8_t> compositePreviewRgba{};
    nicxlive::core::common::SharedArray<bool> missingVertexMask{};
    std::size_t sampledVertices{};
    std::size_t missingVertices{};
    float minDepth{std::numeric_limits<float>::quiet_NaN()};
    float maxDepth{std::numeric_limits<float>::quiet_NaN()};
    bool skipped{};
};
struct PsdDepthImportResult {
    PsdDepthCompositionMode compositionMode{};
    compat::RetainedString compositionModeName{};
    int compositionWidth{};
    int compositionHeight{};
    float globalDepthScale{1.0f};
    float globalDepthCentroid{0.0f};
    PsdDepthMissingPolicy missingPolicy{PsdDepthMissingPolicy::KeepExisting};
    float missingBackDepth{-1.0f};
    std::size_t colorLayerCount{};
    std::size_t sourceDepthLayerCount{};
    std::size_t composedLayerCount{};
    PsdDepthCompositeSource colorSource{};
    PsdDepthCompositeSource depthSource{};
    compat::Slice<PsdDepthCompositionDiagnostic> compositionDiagnostics{};
    compat::Slice<PsdDepthComposedLayer> composedLayers{};
    compat::Slice<PsdDepthLayerMapping> mappings{};
    compat::Slice<PsdDepthGridResult> grids{};
    std::size_t matchedLayers{};
    std::size_t unmatchedLayers{};
    std::size_t ambiguousLayers{};
    std::size_t skippedGrids{};
    bool gpuCompositionRequested{};
    bool smoothWavySurface{};
};
struct PsdDepthSampleResult {
    bool valid{};
    float value{std::numeric_limits<float>::quiet_NaN()};
};
}
