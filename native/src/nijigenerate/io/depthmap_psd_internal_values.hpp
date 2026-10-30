#pragma once
#include "depthmap_psd_composition.hpp"
#include <math/mat4.hpp>
#include <math/veca.hpp>
namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::io::depthmap_psd::detail {
inline nicxlive::core::math::Mat4 initialMatrix() {
    nicxlive::core::math::Mat4 value;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            value[row][column] = std::numeric_limits<float>::quiet_NaN();
    return value;
}
struct CoverageSource {
    int width{};
    int height{};
    int channels{};
    float opacity{1.0f};
    compat::Slice<std::uint8_t> data{};
    nicxlive::core::math::Mat4 worldToLocal{initialMatrix()};
    bool hasDocumentRect{};
    int documentLeft{};
    int documentTop{};
    int documentWidth{};
    int documentHeight{};
    nicxlive::core::math::Mat4 localToDocumentWorld{initialMatrix()};
    nicxlive::core::math::Vec2Array vertices{};
    nicxlive::core::math::Vec2Array uvs{};
    compat::Slice<std::uint16_t> indices{};
    nicxlive::core::math::Vec2 origin{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
};
struct DepthLayerImage {
    compat::RetainedString layerPath{};
    compat::RetainedString layerName{};
    int left{};
    int top{};
    int width{};
    int height{};
    int documentWidth{};
    int documentHeight{};
    float opacity{1.0f};
    compat::Slice<std::uint8_t> data{};
    compat::Slice<float> coverageGateCache{};
    compat::Slice<float> coverageAlphaCache{};
    int coverageWidth{};
    int coverageHeight{};
    int coverageChannels{};
    float coverageOpacity{1.0f};
    compat::Slice<std::uint8_t> coverageData{};
    bool coverageUsesMesh{};
    nicxlive::core::math::Mat4 coverageWorldToLocal{initialMatrix()};
    compat::Slice<CoverageSource> coverageSources{};
    std::shared_ptr<nicxlive::core::nodes::Deformable> grid{};
};
struct Candidate {
    std::shared_ptr<nicxlive::core::nodes::Node> node{};
    int priority{};
    float overlapScore{std::numeric_limits<float>::quiet_NaN()};
};
struct DepthSample {
    bool valid{};
    float value{std::numeric_limits<float>::quiet_NaN()};
    float weight{std::numeric_limits<float>::quiet_NaN()};
};
struct ColorLayerPreviewRgba {
    compat::Slice<std::uint8_t> rgba{};
    int width{};
    int height{};
};
struct GridAccum {
    std::shared_ptr<nicxlive::core::nodes::Deformable> grid{};
    compat::Slice<float> best{};
    compat::Slice<float> baseBest{};
    nicxlive::core::common::SharedArray<bool> has{};
    compat::Slice<compat::RetainedString> winnerLayerPaths{};
    compat::Slice<PsdDepthGridLayerMask> layerMasks{};
};
struct PngComposedBinding {
    DepthLayerImage image{};
    std::shared_ptr<nicxlive::core::nodes::Deformable> target{};
    std::shared_ptr<nicxlive::core::nodes::Node> matchedTarget{};
    compat::RetainedString status{};
    bool manual{};
};
struct CandidateTargetResolution {
    std::shared_ptr<nicxlive::core::nodes::Node> node{};
    std::shared_ptr<nicxlive::core::nodes::Deformable> target{};
    bool ambiguous{};
};
struct PsdClippingBaseState {
    int left{};
    int top{};
    int width{};
    int height{};
    bool visible{};
    float opacity{std::numeric_limits<float>::quiet_NaN()};
    compat::Slice<std::uint8_t> alpha{};
};
}
