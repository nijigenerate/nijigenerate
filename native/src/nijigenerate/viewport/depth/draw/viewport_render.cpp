#include "viewport.hpp"
#include "coordinate.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/viewport.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nodes/deformable.hpp>
#include <bit>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nijigenerate::viewport::depth::draw {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::math::Vec3Array;
using CameraRef = std::shared_ptr<nicxlive::core::math::Camera>;
using TextureRef = std::shared_ptr<nicxlive::core::Texture>;
using namespace core::dbg;
namespace {
compat::RetainedString labelAreaId(std::size_t index) {
    try {
        return compat::RetainedString("DepthDrawLabel" + std::to_string(index));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
Vec3Array DepthDrawViewport::toLineBuffer(compat::Slice<DepthTargetRenderLine> lines) {
    Vec3Array points;
    for (auto line : lines) {
        points.append(Vec3(line.p0.x, line.p0.y, 0.0f));
        points.append(Vec3(line.p1.x, line.p1.y, 0.0f));
    }
    return points;
}

Vec3Array DepthDrawViewport::toPointBuffer(compat::Slice<DepthTargetRenderPoint> points) {
    Vec3Array buffer;
    for (auto point : points) {
        buffer.append(Vec3(point.point.x, point.point.y, 0.0f));
    }
    return buffer;
}

void DepthDrawViewport::drawLines(compat::Slice<DepthTargetRenderLine> lines, Vec4 color, float width) {
    if (lines.size() == 0) return;
    auto points = toLineBuffer(lines);
    inDbgSetBuffer(points);
    inDbgLineWidth(width);
    inDbgDrawLines(color);
    inDbgLineWidth(1.0f);
}

void DepthDrawViewport::drawPoints(compat::Slice<DepthTargetRenderPoint> points, Vec4 color, float size) {
    if (points.size() == 0) return;
    auto buffer = toPointBuffer(points);
    inDbgSetBuffer(buffer);
    inDbgPointsSize(size);
    inDbgDrawPoints(color);
    inDbgPointsSize(1.0f);
}

void DepthDrawViewport::drawRenderGeometry(DepthDrawViewportRenderGeometry& geometry, CameraRef viewportCamera) {
    const auto receiver = weak_from_this().lock();
    const auto targetMeshes = geometry.targetMeshes;
    for (std::size_t i = 0; i < targetMeshes.size(); ++i) {
        const auto mesh = targetMeshes[i];
        TextureRef texture;
        if (i < geometry.targetMeshGridUuids.size()) {
            auto target = compat::ngRequireObject(viewSession)->targetByGrid(geometry.targetMeshGridUuids[i]);
            if (target != nullptr) texture = compat::ngRequireObject(offscreenTextureRenderer)->render(target->getTarget());
        }
        if (texture != nullptr) {
            compat::ngRequireObject(meshRenderer)->draw(texture, mesh.positions, mesh.uvs, mesh.indices, viewportCamera);
        } else {
            compat::ngRequireObject(meshRenderer)->drawSolid(
                mesh.positions, mesh.indices, viewportCamera, Vec4(0.25f, 0.5f, 1.0f, 0.18f));
        }
    }
    drawLines(geometry.normalImageLines, Vec4(0.65f, 0.65f, 0.65f, 0.45f), 1.0f);
    drawLines(geometry.rawDepthLines, Vec4(0.95f, 0.95f, 0.95f, 0.55f), 1.0f);
    drawLines(geometry.coverageLines, Vec4(0.2f, 0.95f, 0.2f, 0.6f), 1.0f);
    compat::Slice<DepthTargetRenderPoint> coverageRenderPoints;
    const auto coveragePoints = geometry.coveragePoints;
    for (auto point : coveragePoints) coverageRenderPoints.append(point.renderPoint);
    drawPoints(coverageRenderPoints, Vec4(0.2f, 1.0f, 0.25f, 0.9f), 3.0f);
    drawLines(geometry.layerPlaneLines, Vec4(0.15f, 0.55f, 1.0f, 0.55f), 1.0f);
    drawLines(geometry.selectedLayerLines, Vec4(1.0f, 1.0f, 0.15f, 1.0f), 2.0f);
    drawLines(geometry.depthRangeLines, Vec4(1.0f, 0.65f, 0.15f, 0.8f), 1.0f);
    drawLines(geometry.gapLines, Vec4(0.2f, 1.0f, 0.9f, 0.9f), 2.0f);
    drawLines(geometry.overlapGapLines, Vec4(1.0f, 0.05f, 0.05f, 1.0f), 3.0f);
    compat::Slice<DepthTargetRenderPoint> gapHandlePoints;
    const auto gapHandles = geometry.gapHandles;
    for (auto handle : gapHandles) gapHandlePoints.append(handle.renderPoint);
    drawPoints(gapHandlePoints, Vec4(0.95f, 1.0f, 0.1f, 1.0f), 7.0f);
    drawLines(geometry.targetLines, Vec4(0.55f, 0.55f, 0.55f, 0.75f), 1.0f);
    drawPoints(geometry.missingPoints, Vec4(1.0f, 0.1f, 0.1f, 1.0f), 6.0f);
    const auto winningPointGroups = geometry.winningPointGroups;
    for (auto group : winningPointGroups) {
        drawPoints(group.points, group.color, 5.0f);
    }
    if (geometry.winningPointGroups.size() == 0) {
        drawPoints(geometry.winningPoints, Vec4(0.1f, 0.9f, 0.35f, 1.0f), 5.0f);
    }
}

void DepthDrawViewport::drawOptions() {
    const auto receiver = weak_from_this().lock();
    if (drawSession == nullptr) return;
    auto display = drawSession->display;
    bool changed{};
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Composite").data(), &display.showComposite) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Layer Planes").data(), &display.showLayerPlanes) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Depth Ranges").data(), &display.showDepthRanges) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Missing Vertices").data(), &display.showMissingVertices) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Winning Layer").data(), &display.showWinningLayer) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Normal Image").data(), &display.showNormalImage) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Raw Depth").data(), &display.showRawDepth) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("Coverage").data(), &display.showCoverage) || changed;
    changed = widgets::ngCheckbox(core::translation::ngTranslateCString("GPU Preview").data(), &display.useGpuPreview) || changed;
    if (changed) compat::ngRequireObject(drawSession)->updateDisplayOptions(display);
}

void DepthDrawViewport::drawConfirmBar() {
    const auto receiver = weak_from_this().lock();
    if (drawSession == nullptr || !drawSession->display.showDepthRanges) return;
    auto geometry = collectRenderGeometry();
    const auto textLabels = geometry.textLabels;
    for (std::size_t i = 0; i < textLabels.size(); ++i) {
        const auto label = textLabels[i];
        auto viewportPoint = core::input::WorldToViewport(label.point.x, label.point.y);
        widgets::ngBeginViewportToolArea(
            compat::RetainedStringSlice(labelAreaId(i)),
            ImVec2(viewportPoint.x + 8.0f, viewportPoint.y - 8.0f),
            true
        );
        widgets::ngTextLabel(compat::RetainedStringSlice(label.text));
        widgets::ngEndViewportToolArea();
    }
}

void DepthDrawViewport::present() {
    const auto receiver = weak_from_this().lock();
    syncTargets();
}

void DepthDrawViewport::withdraw() {
    const auto receiver = weak_from_this().lock();
    previewResults = {};
    cancelPendingGpuPreviews();
    gpuPreviewErrorMessage = {};
    compat::ngRequireObject(viewSession)->clear();
}

void DepthDrawViewport::selectionChanged(compat::Slice<NodeRef> selection) {
    const auto receiver = weak_from_this().lock();
    this->selection = selection;
    syncTargets();
}

void DepthDrawViewport::draw(CameraRef camera) {
    const auto receiver = weak_from_this().lock();
    composeDirtyPreviews();
    auto geometry = collectRenderGeometry();
    drawRenderGeometry(geometry, camera);
}

void DepthDrawViewport::update(ImGuiIO* io, CameraRef camera) {
    const auto receiver = weak_from_this().lock();
    composeSelectedPreviewForUpdate();
    if (drawSession != nullptr && core::input::ngInputIsMouseClicked(ImGuiMouseButton_Left)) {
        auto mouse = core::input::ngInputGetMousePosition();
        Vec2 point{-mouse.x, -mouse.y};
        if (!selectTargetAtDepthViewPoint(point)) {
            selectLayerPlaneAtDepthViewPoint(point);
        }
    }
}
}
