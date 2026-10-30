#pragma once

#include "nijigenerate/viewport/depth/common/targetview.hpp"
#include "nijigenerate/viewport/depth/draw/layer.hpp"
#include <cstdint>
#include <memory>

namespace nijigenerate::viewport::depth::draw::coordinate {

void ngDepthDrawLayerDocumentBounds(const DepthDrawLayer& layer,
    nicxlive::core::math::Vec2& minPoint, nicxlive::core::math::Vec2& maxPoint);
nicxlive::core::math::Vec2 ngDepthDrawVertexDocumentPosition(
    std::shared_ptr<common::targetview::DepthTargetView> target,
    nicxlive::core::math::Vec2 vertex, std::int32_t documentWidth, std::int32_t documentHeight);
nicxlive::core::math::Vec2 ngDepthDrawLayerPixelFromDocument(
    DepthDrawLayer layer, nicxlive::core::math::Vec2 documentPoint);
nicxlive::core::math::Vec2 ngDepthDrawLayerPixelFromVertex(
    std::shared_ptr<common::targetview::DepthTargetView> target, DepthDrawLayer layer,
    nicxlive::core::math::Vec2 vertex, std::int32_t documentWidth, std::int32_t documentHeight);

} // namespace nijigenerate::viewport::depth::draw::coordinate
