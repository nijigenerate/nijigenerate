#pragma once

#include "depthmap_psd_types.hpp"
#include <nodes/node.hpp>
#include <nodes/deformable.hpp>
#include <nodes/grid_deformer.hpp>
#include <memory>

namespace nijigenerate::io::depthmap_psd {

std::shared_ptr<nicxlive::core::nodes::GridDeformer> ngPsdDepthContainingGrid(
    std::shared_ptr<nicxlive::core::nodes::Node> node);
std::shared_ptr<nicxlive::core::nodes::Deformable> ngPsdDepthContainingTarget(
    std::shared_ptr<nicxlive::core::nodes::Node> node);
compat::RetainedString ngPsdDepthGridLayerKey(std::uint64_t gridUuid, compat::RetainedString layerPath);
bool ngPsdDepthGridEnabled(PsdDepthImportSettings& settings, std::uint64_t gridUuid);
bool ngPsdDepthGridLayerEnabled(PsdDepthImportSettings& settings, std::uint64_t gridUuid,
    compat::RetainedString layerPath);
nicxlive::core::math::Vec2 ngPsdDepthGridVertexDocumentPosition(
    std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid, nicxlive::core::math::Vec2 vertex,
    int documentWidth, int documentHeight);
nicxlive::core::math::Vec2 ngPsdDepthTargetPointDocumentPosition(
    std::shared_ptr<nicxlive::core::nodes::Deformable> target, nicxlive::core::math::Vec2 vertex,
    int documentWidth, int documentHeight);

} // namespace nijigenerate::io::depthmap_psd
