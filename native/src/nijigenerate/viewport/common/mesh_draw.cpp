// Copyright 2022 Inochi2D Project, 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "mesh.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/compat/array_access.hpp>

namespace nijigenerate::viewport::common::mesh {
namespace { Vec3 point3(Vec2 point) { return {point.x, point.y, 0}; } }
void IncMesh::drawLines(Mat4 transform, Vec4 color) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!lines_.empty()) { core::dbg::inDbgSetBuffer(lines_); core::dbg::inDbgDrawLines(color, transform); }
    if (!wlines_.empty()) {
        core::dbg::inDbgSetBuffer(wlines_); core::dbg::inDbgDrawLines({0.7f, 0.2f, 0.2f, 1}, transform);
    }
}
void IncMesh::drawPoints(Mat4 transform, Vec4 color, Markers markers) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (points_.empty()) return;
    core::dbg::inDbgSetBuffer(points_);
    core::dbg::inDbgPointsSize(10);
    core::dbg::inDbgDrawPoints({0, 0, 0, 1}, transform);
    for (auto marker : markers) {
        Vec3Array selected;
        for (const auto index : marker.first) {
            selected.push_back(points_[static_cast<std::size_t>(index)].toVector());
        }
        core::dbg::inDbgSetBuffer(selected);
        core::dbg::inDbgPointsSize(10);
        core::dbg::inDbgDrawPoints(marker.second, transform);
    }
    core::dbg::inDbgSetBuffer(points_);
    core::dbg::inDbgPointsSize(6);
    core::dbg::inDbgDrawPoints(color, transform);
}
void IncMesh::drawPointSubset(MeshVertexArray subset, Vec4 color, Mat4 transform, float size) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (subset.empty()) return;
    Vec3Array points;
    for (auto* vertex : subset) if (vertex) points.push_back(point3(vertex->position));
    core::dbg::inDbgSetBuffer(points); core::dbg::inDbgPointsSize(size); core::dbg::inDbgDrawPoints(color, transform);
}
void IncMesh::drawPoint(Vec2 point, Vec4 color, Mat4 transform, float size) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Vec3Array points;
    points.push_back(point3(point));
    core::dbg::inDbgSetBuffer(points); core::dbg::inDbgPointsSize(size); core::dbg::inDbgDrawPoints(color, transform);
}
void IncMesh::draw(Mat4 transform, Vec4 vertexColor, Vec4 edgeColor, Markers markers) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    drawLines(transform, edgeColor);
    drawPoints(transform, vertexColor, markers);
}
}
