// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "operation.hpp"
#include "../mesheditor/node.hpp"
#include "../renderer.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/ext/nodes/exdepthops.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/grid_deformer.hpp>
#include <compat/source_allocator.hpp>
#include <SDL3/SDL_opengl.h>
#include <tuple>

namespace nijigenerate::actions::node::detail {
// Existing native null-object fault implementation in actions/node.cpp.
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::viewport::depth::tools::operation {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec4;
using nicxlive::core::math::Vec3Array;
using nicxlive::core::d_runtime::SourceAllocator;
namespace dbg = core::dbg;
void drawDepthPoint(Vec2 point, Vec4 color, float size) {
    const auto enabled = glIsEnabled(GL_DEPTH_TEST); glDisable(GL_DEPTH_TEST);
    Vec3Array points{nicxlive::core::math::Vec3{point.x, point.y, 0}}; dbg::inDbgSetBuffer(points);
    dbg::inDbgPointsSize(size); dbg::inDbgDrawPoints(color);
    if (enabled) glEnable(GL_DEPTH_TEST);
}
void drawDepthPoint(DepthEditorHandle editor, Vec2 local, float depth, DepthCamera3D& camera, Vec4 color, float size) {
    const auto renderer = std::allocate_shared<DepthTargetRenderer>(SourceAllocator<DepthTargetRenderer>{});
    if (!editor) actions::node::detail::failActionObjectReference();
    const auto point = renderer->buildPoint(local, depth, editor->depthDisplayScale(), camera, size);
    drawDepthPoint(point.point, color, point.size);
}
void drawDepthLinePoints(Vec2 p0, Vec2 p1, Vec4 color, float width) {
    const auto enabled = glIsEnabled(GL_DEPTH_TEST); glDisable(GL_DEPTH_TEST);
    Vec3Array points; points.append({p0.x, p0.y, 0}); points.append({p1.x, p1.y, 0});
    dbg::inDbgSetBuffer(points); dbg::inDbgLineWidth(width); dbg::inDbgDrawLines(color); dbg::inDbgLineWidth(1);
    if (enabled) glEnable(GL_DEPTH_TEST);
}
void drawDepthLine(DepthEditorHandle editor, Vec2 p0, Vec2 p1, DepthCamera3D& camera, Vec4 color, float width) {
    const auto renderer = std::allocate_shared<DepthTargetRenderer>(SourceAllocator<DepthTargetRenderer>{});
    if (!editor) actions::node::detail::failActionObjectReference();
    const auto line = renderer->buildLine(p0, p1, 0, editor->depthDisplayScale(), camera);
    drawDepthLinePoints(line.p0, line.p1, color, width);
}
void drawDepthPolyline(Vec3Array points, Vec4 color, float width) {
    if (points.size() < 2) return;
    const auto enabled = glIsEnabled(GL_DEPTH_TEST); glDisable(GL_DEPTH_TEST);
    Vec3Array lines;
    for (std::size_t i = 0; i < points.size() - 1; ++i) {
        lines.append(points[i].toVector()); lines.append(points[i + 1].toVector());
    }
    dbg::inDbgSetBuffer(lines); dbg::inDbgLineWidth(width); dbg::inDbgDrawLines(color); dbg::inDbgLineWidth(1);
    if (enabled) glEnable(GL_DEPTH_TEST);
}
void drawDepthRingCurve(DepthEditorHandle editor, std::shared_ptr<DepthRingOperation> operation,
    DepthCamera3D& camera, bool backSide, Vec4 color, float width) {
    Vec3Array points;
    for (int i = 0; i <= 48; ++i) {
        const auto point = ringCurvePoint(editor, operation, float(i) / 48, backSide, camera);
        points.append({point.x, point.y, 0});
    }
    drawDepthPolyline(points, color, width);
}
void drawDepthEllipse(DepthEditorHandle editor, Vec2 center, float radiusX, float radiusY, float angle,
    float depth, DepthCamera3D& camera, Vec4 color, float width) {
    const auto enabled = glIsEnabled(GL_DEPTH_TEST); glDisable(GL_DEPTH_TEST);
    Vec3Array points;
    const auto renderer = std::allocate_shared<DepthTargetRenderer>(SourceAllocator<DepthTargetRenderer>{});
    if (!editor) actions::node::detail::failActionObjectReference();
    for (auto line : renderer->buildEllipseLines(center, radiusX, radiusY, angle, depth, editor->depthDisplayScale(), camera)) {
        points.append({line.p0.x, line.p0.y, 0}); points.append({line.p1.x, line.p1.y, 0});
    }
    dbg::inDbgSetBuffer(points); dbg::inDbgLineWidth(width); dbg::inDbgDrawLines(color); dbg::inDbgLineWidth(1);
    if (enabled) glEnable(GL_DEPTH_TEST);
}
void DepthAttachedPointOperation::draw(DepthEditorHandle editor, DepthCamera3D& camera, bool selected, DepthOperationHandle hot) {
    const auto keepAlive = retainReceiver();
    if (!editor) actions::node::detail::failActionObjectReference();
    const auto point = editor->localVertex(index), base = editor->projectLocalPoint(point, 0, camera);
    const auto projected = editor->projectLocalPoint(point, amount, camera);
    const bool hotAmount = hot == DepthOperationHandle::Amount, hotBody = hot == DepthOperationHandle::Body;
    const auto color = depthOperationColor(amount, selected || hotAmount);
    drawDepthLinePoints(base, projected, color, hotAmount ? 3.f : 1.8f);
    drawDepthPoint(editor, point, 0, camera, hotBody ? DepthOperationAmountColor : DepthOperationHandleColor, hotBody ? 13 : 8);
    drawDepthPoint(editor, point, amount, camera, hotAmount ? DepthOperationAmountColor : color, hotAmount ? 15 : selected ? 13 : 9);
}
void DepthRingOperation::draw(DepthEditorHandle editor, DepthCamera3D& camera, bool selected, DepthOperationHandle hot) {
    const auto self = std::static_pointer_cast<DepthRingOperation>(retainedSelf());
    const bool body = hot == DepthOperationHandle::Body; const auto color = depthOperationColor(amount, selected || body);
    drawDepthRingCurve(editor, self, camera, false, color, body ? 4.5f : 2.5f);
    const auto lineStart = ringEndpointPoint(editor, self, true, camera);
    const auto lineEnd = ringEndpointPoint(editor, self, false, camera);
    drawDepthLinePoints(lineStart, lineEnd, {.55f, .55f, .55f, .55f}, 1.2f);
    if (!selected && hot == DepthOperationHandle::None) return;
    const auto p0h = ringAngleHandlePoint(editor, self, true, camera), p1h = ringAngleHandlePoint(editor, self, false, camera);
    drawDepthLinePoints(ringEndpointPoint(editor, self, true, camera), p0h, DepthOperationHandleColor,
        hot == DepthOperationHandle::P0Angle ? 2.8f : 1.4f);
    drawDepthLinePoints(ringEndpointPoint(editor, self, false, camera), p1h, DepthOperationHandleColor,
        hot == DepthOperationHandle::P1Angle ? 2.8f : 1.4f);
    drawDepthPoint(ringEndpointPoint(editor, self, true, camera), hot == DepthOperationHandle::P0 ? DepthOperationAmountColor : DepthOperationHandleColor,
        hot == DepthOperationHandle::P0 ? 13 : 9);
    drawDepthPoint(ringEndpointPoint(editor, self, false, camera), hot == DepthOperationHandle::P1 ? DepthOperationAmountColor : DepthOperationHandleColor,
        hot == DepthOperationHandle::P1 ? 13 : 9);
    drawDepthPoint(ringAmountHandlePoint(editor, self, camera), hot == DepthOperationHandle::Amount ? DepthOperationAmountColor : color,
        hot == DepthOperationHandle::Amount ? 14 : 10);
    drawDepthPoint(p0h, hot == DepthOperationHandle::P0Angle ? DepthOperationAmountColor : DepthOperationHandleColor,
        hot == DepthOperationHandle::P0Angle ? 13 : 8);
    drawDepthPoint(p1h, hot == DepthOperationHandle::P1Angle ? DepthOperationAmountColor : DepthOperationHandleColor,
        hot == DepthOperationHandle::P1Angle ? 13 : 8);
}
void DepthPlaneOperation::draw(DepthEditorHandle editor, DepthCamera3D& camera, bool selected, DepthOperationHandle hot) {
    const auto keepAlive = retainReceiver(); const bool body = hot == DepthOperationHandle::Body;
    const auto color = depthOperationColor(targetDepth, selected || body);
    drawDepthEllipse(editor, center, radiusX, radiusY, angle, targetDepth, camera, color, body ? 4.f : 2.f);
    if (!selected && hot == DepthOperationHandle::None) return;
    const float radians = angle * 3.14159265358979323846f / 180.f;
    const Vec2 ux{std::cos(radians), std::sin(radians)}, uy{-std::sin(radians), std::cos(radians)};
    drawDepthPoint(editor->projectLocalPoint(center, targetDepth, camera), hot == DepthOperationHandle::Amount ? DepthOperationAmountColor : color,
        hot == DepthOperationHandle::Amount ? 14 : 10);
    drawDepthPoint(editor->projectLocalPoint({center.x + ux.x * radiusX, center.y + ux.y * radiusX}, targetDepth, camera),
        hot == DepthOperationHandle::RadiusX ? DepthOperationAmountColor : DepthOperationHandleColor, hot == DepthOperationHandle::RadiusX ? 13 : 9);
    drawDepthPoint(editor->projectLocalPoint({center.x + uy.x * radiusY, center.y + uy.y * radiusY}, targetDepth, camera),
        hot == DepthOperationHandle::RadiusY ? DepthOperationAmountColor : DepthOperationHandleColor, hot == DepthOperationHandle::RadiusY ? 13 : 9);
}
compat::Slice<float> ngComputeDepthsFromOps(std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid,
    compat::Slice<ext::nodes::ExDepthOp> operations, compat::ConstSlice<float> baseDepths) {
    const auto editor = mesheditor::node::DepthMeshEditorOne::createSource([&] {
        return std::tuple{grid, false};
    });
    compat::Slice<float> result;
    compat::ngRunWithScopeExit([&] {
        if (baseDepths.size() == compat::ngRequireObject(grid)->getVertices().size()) {
            compat::Slice<float> copied;
            if (!baseDepths.empty()) {
                try {
                    copied = compat::Slice<float>(baseDepths.begin(), baseDepths.end());
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
                }
            }
            editor->replaceBaseDepths(copied);
        } else editor->clearBaseDepths();
        editor->resetWorkingDepths();
        compat::Slice<std::shared_ptr<DepthRingOperation>> rings;
        compat::Slice<std::shared_ptr<DepthAttachedPointOperation>> attached;
        compat::Slice<std::shared_ptr<DepthPlaneOperation>> planes;
        for (auto op : operations) {
            const auto operation = depthOperationFromExDepthOp(op);
            if (auto ring = std::dynamic_pointer_cast<DepthRingOperation>(operation)) rings.append(ring);
            else if (auto point = std::dynamic_pointer_cast<DepthAttachedPointOperation>(operation)) attached.append(point);
            else if (auto plane = std::dynamic_pointer_cast<DepthPlaneOperation>(operation)) planes.append(plane);
        }
        applyRingNormalSurfaces(editor, rings);
        for (auto point : attached) compat::ngRequireObject(point)->apply(editor);
        for (auto plane : planes) compat::ngRequireObject(plane)->apply(editor);
        result = editor->copyEditorDepths();
    }, [&] { editor->dispose(); });
    return result;
}
}
