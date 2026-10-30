// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
// Used inmath vector length: David Herberth and Inochi2D Project, MIT.
// Permission/disclaimer: port/licenses/inmath-depthbone-quaternion.txt.
#include "ring.hpp"
#include "../viewport.hpp"
#include "../mesheditor/node.hpp"
#include "../mesheditor/editor.hpp"
#include <nijigenerate/commands/depth/editor_commands.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <cmath>
#include <imgui.h>

namespace nijigenerate::viewport::depth::tools::ring {
namespace {
[[maybe_unused]] const bool ringSourceClassInfo = [] {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthRingTool),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.viewport.depth.tools.ring.DepthRingTool"));
    return true;
}();
}
namespace {
#ifdef _WIN32
using RingSourceReal = double;
#else
using RingSourceReal = long double;
#endif
RingSourceReal pointDistance(nicxlive::core::math::Vec2 left, nicxlive::core::math::Vec2 right) {
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    // inmath accumulates float products in the platform D real type.
    RingSourceReal squared = 0;
    squared += dx * dx;
    squared += dy * dy;
    return std::sqrt(squared);
}
}
bool DepthRingTool::isRingOperation(std::shared_ptr<operation::DepthOperation> value) {
    return std::dynamic_pointer_cast<operation::DepthRingOperation>(value) != nullptr;
}
void DepthRingTool::resetCreateDrag() { activeEditor.reset(); startVertex = -1; currentVertex = -1; }
void DepthRingTool::setMode(RingToolMode mode) {
    if (toolMode == mode) return;
    toolMode = mode;
    resetCreateDrag();
}
void DepthRingTool::toggleMode() {
    setMode(toolMode == RingToolMode::Create ? RingToolMode::Edit : RingToolMode::Create);
}
bool DepthRingTool::nearest(ImGuiIO* io, Camera camera, Viewport viewport, Editor& editor,
    std::ptrdiff_t& index) {
    editor.reset();
    index = 0;
    editor = nearestVertexFromScreenMouse(io, camera, viewport, index);
    return editor && index >= 0;
}
DepthRingTool::Vec2 DepthRingTool::localMouse(ImGuiIO* io, Camera camera, Viewport viewport) {
    return screenMouseToModel(io, camera, activeEditor, viewport);
}
std::shared_ptr<operation::DepthRingOperation> DepthRingTool::buildOperation(Viewport viewport) {
    if (!activeEditor || startVertex < 0) return nullptr;
    auto p0 = startPoint;
    auto p1 = currentPoint;
    if (pointDistance(p1, p0) < 1.0f) {
        const auto settings = compat::ngRequireObject(viewport)->brushSettings();
        const auto angle = settings.angle * 3.14159265358979323846f / 180.0f;
        const Vec2 d{std::cos(angle) * settings.radiusX, std::sin(angle) * settings.radiusX};
        p0 = {p0.x - d.x, p0.y - d.y};
        p1 = {p1.x + d.x, p1.y + d.y};
    }
    return operation::DepthRingOperation::createSource([&] {
        const auto first = p0;
        const auto second = p1;
        const auto settings = compat::ngRequireObject(viewport)->brushSettings();
        return std::tuple{first, second, settings};
    });
}
DepthToolMode DepthRingTool::mode() { return DepthToolMode::Ring; }
const char* DepthRingTool::icon() { return "\xEF\x9C\x9A"; }
compat::RetainedString DepthRingTool::tooltip() {
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Edit Depth Ring"));
}
void DepthRingTool::draw(Camera, Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    if (!activeEditor || startVertex < 0) return;
    const auto p0 = startPoint;
    auto p1 = currentPoint;
    if (pointDistance(p1, p0) < 1.0f) {
        const auto settings = compat::ngRequireObject(viewport)->brushSettings();
        const auto angle = settings.angle * 3.14159265358979323846f / 180.0f;
        p1 = {p0.x + std::cos(angle) * settings.radiusX, p0.y + std::sin(angle) * settings.radiusX};
    }
    const auto target = activeEditor;
    auto& depthCamera = compat::ngRequireObject(viewport)->depthCameraState();
    const auto color = operation::depthOperationColor(compat::ngRequireObject(viewport)->brushSettings().amount, true);
    operation::drawDepthLine(target, p0, p1, depthCamera, color);
}
void DepthRingTool::drawOptions(Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const auto createLabel = core::translation::ngTranslateCString("Create");
    if (widgets::ngButtonColored(createLabel.data(), ImVec2(0, 0),
        toolMode == RingToolMode::Create ? widgets::colorUndefined : ImVec4(.6f, .6f, .6f, 1)))
        setMode(RingToolMode::Create);
    widgets::ngTooltip(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Create depth rings")));
    ImGui::SameLine(0, 0);
    const auto editLabel = core::translation::ngTranslateCString("Edit");
    if (widgets::ngButtonColored(editLabel.data(), ImVec2(0, 0),
        toolMode == RingToolMode::Edit ? widgets::colorUndefined : ImVec4(.6f, .6f, .6f, 1)))
        setMode(RingToolMode::Edit);
    widgets::ngTooltip(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Edit existing depth rings")));
    ImGui::PopStyleVar();
    ImGui::SameLine(0, 6);
    auto& settings = compat::ngRequireObject(viewport)->brushSettings();
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Amount"), &settings.amount,
        .01f, -2.f, 2.f, compat::RetainedString::fromStaticLiteral("%.2f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Length"), &settings.radiusX,
        1.f, 1.f, 1000.f, compat::RetainedString::fromStaticLiteral("%.0f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Width"), &settings.radiusY,
        1.f, 1.f, 400.f, compat::RetainedString::fromStaticLiteral("%.0f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Angle"), &settings.angle,
        1.f, -180.f, 180.f, compat::RetainedString::fromStaticLiteral("%.0f"));
    drawOptionDrag(compat::RetainedString::fromStaticLiteral("Falloff"), &settings.hardness,
        .05f, .1f, 8.f, compat::RetainedString::fromStaticLiteral("%.2f"), false);
}
bool DepthRingTool::update(ImGuiIO* io, Camera camera, Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    if (core::input::ngInputIsKeyPressed(ImGuiKey_Tab)) { toggleMode(); return true; }
    if (toolMode == RingToolMode::Edit) {
        const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
        if (!editorSet) return false;
        if (compat::ngRequireObject(io)->MouseDown[0]) {
            const auto mouse = screenMouseToView(io, camera);
            const auto mouseY = compat::ngRequireObject(io)->MousePos.y;
            auto& depthCamera = compat::ngRequireObject(viewport)->depthCameraState();
            const auto snapToGrid = compat::ngRequireObject(viewport)->brushSettings().snapToGrid;
            if (editorSet->updateOperationDrag(mouse, mouseY, depthCamera, snapToGrid)) return true;
        }
        if (!compat::ngRequireObject(io)->MouseDown[0] && editorSet->endOperationDrag()) return true;
        if (!core::input::ngInputIsMouseClicked(ImGuiMouseButton_Left)) return false;
        const auto mouse = screenMouseToView(io, camera);
        const auto mouseY = compat::ngRequireObject(io)->MousePos.y;
        auto& depthCamera = compat::ngRequireObject(viewport)->depthCameraState();
        auto filter = [receiver = shared_from_this()](std::shared_ptr<operation::DepthOperation> value) {
            return receiver->isRingOperation(std::move(value));
        };
        return editorSet->beginOperationDrag(mouse, mouseY, depthCamera, std::move(filter));
    }
    if (activeEditor) {
        if (compat::ngRequireObject(io)->MouseDown[0]) {
            Editor editor;
            std::ptrdiff_t index;
            if (nearest(io, camera, viewport, editor, index) && editor == activeEditor) {
                currentVertex = index;
                if (compat::ngRequireObject(viewport)->brushSettings().snapToGrid) {
                    const auto receiver = activeEditor;
                    currentPoint = compat::ngRequireObject(receiver)->localVertex(static_cast<std::size_t>(index));
                } else {
                    currentPoint = localMouse(io, camera, viewport);
                }
            }
            return true;
        }
        const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
        if (const auto op = buildOperation(viewport)) {
            using namespace commands::depth::editor;
            auto context = std::allocate_shared<commands::Context>(
                nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
            const auto target = activeEditor;
            ngRunDepthEditorCommand<DepthEditorOperationCommand::AddEditorDepthOp>(
                context, editorSet, target, op, -1);
        }
        resetCreateDrag();
        return true;
    }
    if (!core::input::ngInputIsMouseClicked(ImGuiMouseButton_Left)) return false;
    std::ptrdiff_t index;
    if (!nearest(io, camera, viewport, activeEditor, index)) return false;
    startVertex = index;
    currentVertex = index;
    if (compat::ngRequireObject(viewport)->brushSettings().snapToGrid) {
        const auto receiver = activeEditor;
        startPoint = compat::ngRequireObject(receiver)->localVertex(static_cast<std::size_t>(index));
    } else {
        startPoint = localMouse(io, camera, viewport);
    }
    currentPoint = startPoint;
    return true;
}
}
