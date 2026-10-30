#include "nijigenerate/viewport/depth/draw/coordinate.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <math/mat4.hpp>
#include <nodes/deformable.hpp>
#include <algorithm>
#include <limits>

namespace nijigenerate::viewport::depth::draw::coordinate {
using nicxlive::core::math::Vec2;

void ngDepthDrawLayerDocumentBounds(const DepthDrawLayer& layer, Vec2& minPoint, Vec2& maxPoint) {
    // D out parameters are reset before the body, including when they alias layer fields.
    const float initial = std::numeric_limits<float>::quiet_NaN();
    minPoint = {initial, initial};
    maxPoint = {initial, initial};
    const float scaleX = layer.xyScale.x == 0.0f ? 1.0f : layer.xyScale.x;
    const float scaleY = layer.xyScale.y == 0.0f ? 1.0f : layer.xyScale.y;
    const float x0 = static_cast<float>(layer.bounds.left) + layer.xyOffset.x;
    const float y0 = static_cast<float>(layer.bounds.top) + layer.xyOffset.y;
    const float x1 = x0 + static_cast<float>(layer.bounds.width) * scaleX;
    const float y1 = y0 + static_cast<float>(layer.bounds.height) * scaleY;
    minPoint = {std::min(x0, x1), std::min(y0, y1)};
    maxPoint = {std::max(x0, x1), std::max(y0, y1)};
}

Vec2 ngDepthDrawVertexDocumentPosition(std::shared_ptr<common::targetview::DepthTargetView> target,
    Vec2 vertex, std::int32_t documentWidth, std::int32_t documentHeight) {
    const auto grid = compat::ngRequireObject(target)->getTarget();
    const auto matrix = compat::ngRequireObject(grid)->transform().matrix();
    // Use the actual runtime's inmath scalar fold, retaining zero Z and unit W terms.
    const float values[]{vertex.x, vertex.y, 0.0f, 1.0f};
    float world[4]{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            world[row] = nicxlive::core::math::detail::inmathAccumulate(world[row],
                nicxlive::core::math::detail::inmathProduct(matrix[row][column], values[column]));
        }
    }
    return {world[0] + static_cast<float>(documentWidth) / 2.0f,
        world[1] + static_cast<float>(documentHeight) / 2.0f};
}

Vec2 ngDepthDrawLayerPixelFromDocument(DepthDrawLayer layer, Vec2 documentPoint) {
    const float scaleX = layer.xyScale.x == 0.0f ? 1.0f : layer.xyScale.x;
    const float scaleY = layer.xyScale.y == 0.0f ? 1.0f : layer.xyScale.y;
    const Vec2 raw{documentPoint.x - static_cast<float>(layer.bounds.left),
        documentPoint.y - static_cast<float>(layer.bounds.top)};
    return {(raw.x - layer.xyOffset.x) / scaleX, (raw.y - layer.xyOffset.y) / scaleY};
}

Vec2 ngDepthDrawLayerPixelFromVertex(std::shared_ptr<common::targetview::DepthTargetView> target,
    DepthDrawLayer layer, Vec2 vertex, std::int32_t documentWidth, std::int32_t documentHeight) {
    const auto documentPoint = ngDepthDrawVertexDocumentPosition(target, vertex, documentWidth, documentHeight);
    return ngDepthDrawLayerPixelFromDocument(layer, documentPoint);
}

} // namespace nijigenerate::viewport::depth::draw::coordinate
