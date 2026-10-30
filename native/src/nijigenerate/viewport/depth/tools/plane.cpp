// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "plane.hpp"
#include "../viewport.hpp"
#include "../mesheditor/node.hpp"
#include <nijigenerate/commands/depth/editor_commands.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace nijigenerate::viewport::depth::tools::plane {
namespace {
[[maybe_unused]] const bool planeSourceClassInfo = [] {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthPlaneTool),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.viewport.depth.tools.plane.DepthPlaneTool"));
    return true;
}();
}
bool DepthPlaneTool::nearest(ImGuiIO* io, Camera camera, Viewport viewport, Editor& editor,
    std::ptrdiff_t& index) {
    editor.reset();
    index = 0;
    editor = nearestVertexFromScreenMouse(io, camera, viewport, index);
    return editor && index >= 0;
}
DepthPlaneTool::Vec2 DepthPlaneTool::center() { return centerPoint; }
DepthPlaneTool::Vec2 DepthPlaneTool::localMouse(ImGuiIO* io, Camera camera, Viewport viewport) {
    return screenMouseToModel(io, camera, activeEditor, viewport);
}
void DepthPlaneTool::updateRadius(ImGuiIO* io, Camera camera, Viewport viewport) {
    Editor editor;
    std::ptrdiff_t index;
    if (!nearest(io, camera, viewport, editor, index) || editor != activeEditor) return;
    const auto c = center();
    Vec2 p;
    if (compat::ngRequireObject(viewport)->brushSettings().snapToGrid) {
        const auto receiver = activeEditor;
        p = compat::ngRequireObject(receiver)->localVertex(static_cast<std::size_t>(index));
    } else {
        p = localMouse(io, camera, viewport);
    }
    radiusX = std::max(4.0f, std::abs(p.x - c.x));
    radiusY = std::max(4.0f, std::abs(p.y - c.y));
}
void DepthPlaneTool::apply(Viewport viewport) {
    if (!activeEditor || centerVertex < 0) return;
    using namespace commands::depth::editor;
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
    auto context = std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    // Capture source value arguments before later virtual viewport calls.
    const auto target = activeEditor;
    auto depthOperation = operation::DepthPlaneOperation::createSource([&] {
        const auto origin = center();
        const auto operationRadiusX = radiusX;
        const auto operationRadiusY = radiusY;
        const auto settings = compat::ngRequireObject(viewport)->brushSettings();
        return std::tuple{origin, operationRadiusX, operationRadiusY, settings};
    });
    ngRunDepthEditorCommand<DepthEditorOperationCommand::AddEditorDepthOp>(
        context, editorSet, target, depthOperation, -1);
}
DepthToolMode DepthPlaneTool::mode() { return DepthToolMode::Plane; }
const char* DepthPlaneTool::icon() { return "\xEF\x94\x87"; }
compat::RetainedString DepthPlaneTool::tooltip() {
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Flatten Depth Plane"));
}
void DepthPlaneTool::draw(Camera, Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    if (!activeEditor || centerVertex < 0) return;
    const auto target = activeEditor;
    const auto origin = center();
    const auto drawRadiusX = radiusX;
    const auto drawRadiusY = radiusY;
    const auto angle = compat::ngRequireObject(viewport)->brushSettings().angle;
    const auto amount = compat::ngRequireObject(viewport)->brushSettings().amount;
    auto& camera = compat::ngRequireObject(viewport)->depthCameraState();
    const auto color = operation::depthOperationColor(compat::ngRequireObject(viewport)->brushSettings().amount, true);
    operation::drawDepthEllipse(target, origin, drawRadiusX, drawRadiusY, angle, amount, camera, color);
}
void DepthPlaneTool::drawOptions(Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    auto& settings = compat::ngRequireObject(viewport)->brushSettings();
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Target"), &settings.amount,
        .01f, -2.f, 2.f, compat::RetainedString::fromStaticLiteral("%.2f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Radius X"), &settings.radiusX,
        1.f, 1.f, 1000.f, compat::RetainedString::fromStaticLiteral("%.0f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Radius Y"), &settings.radiusY,
        1.f, 1.f, 1000.f, compat::RetainedString::fromStaticLiteral("%.0f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Angle"), &settings.angle,
        1.f, -180.f, 180.f, compat::RetainedString::fromStaticLiteral("%.0f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Flatten"), &settings.flattenStrength,
        .01f, 0.f, 1.f, compat::RetainedString::fromStaticLiteral("%.2f"), false);
}
bool DepthPlaneTool::update(ImGuiIO* io, Camera camera, Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    if (activeEditor) {
        if (compat::ngRequireObject(io)->MouseDown[0]) { updateRadius(io, camera, viewport); return true; }
        apply(viewport);
        activeEditor.reset();
        centerVertex = -1;
        return true;
    }
    if (!core::input::ngInputIsMouseClicked(ImGuiMouseButton_Left)) return false;
    std::ptrdiff_t index;
    if (!nearest(io, camera, viewport, activeEditor, index)) return false;
    centerVertex = index;
    if (compat::ngRequireObject(viewport)->brushSettings().snapToGrid) {
        const auto receiver = activeEditor;
        centerPoint = compat::ngRequireObject(receiver)->localVertex(static_cast<std::size_t>(index));
    } else {
        centerPoint = localMouse(io, camera, viewport);
    }
    radiusX = compat::ngRequireObject(viewport)->brushSettings().radiusX;
    radiusY = compat::ngRequireObject(viewport)->brushSettings().radiusY;
    return true;
}
}
