#include "viewport.hpp"
#include "mesheditor/editor.hpp"
#include "tools/package.hpp"
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_allocator.hpp>
#include <puppet.hpp>
#include <nodes/grid_deformer.hpp>
#include <limits>
#include <tuple>

namespace nijigenerate::viewport::depth::viewport {
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::GridDeformer;
using nicxlive::core::math::Camera;

void DepthEditViewport::syncTargets() {
    const auto keepAlive = weak_from_this().lock();
    if (editor) {
        const auto receiver = compat::ngRequireObject(editor);
        receiver->setTargets(selection);
    }
}
void DepthEditViewport::leaveToModel(compat::Slice<std::shared_ptr<GridDeformer>> targets) {
    const auto keepAlive = weak_from_this().lock();
    project::incSetEditMode(project::EditMode::ModelEdit);
    for (auto target : targets) project::incAddSelectNode(target);
    if (!targets.empty()) focusGridOrigin(targets[0]);
}
bool DepthEditViewport::updateDepthCamera(ImGuiIO* io) {
    if (!io) compat::ngFailNullAccess(false);
    const bool rotate = io->MouseDown[1] && !io->KeyShift &&
        core::input::ngInputIsDragRequested(ImGuiMouseButton_Right);
    const bool zoom = io->MouseWheel != 0;
    return ngUpdateDepthCamera3D(depthCamera, io, rotate, false, zoom);
}
void DepthEditViewport::present() {
    const auto keepAlive = weak_from_this().lock();
    editor = mesheditor::editor::DepthMeshEditor::createSource([] { return std::tuple{}; });
    syncTargets();
    const auto targets = compat::ngRequireObject(editor)->getTargets();
    if (!targets.empty()) focusGridOrigin(targets[0]);
}
void DepthEditViewport::withdraw() {
    const auto keepAlive = weak_from_this().lock();
    if (editor) {
        {
            const auto receiver = compat::ngRequireObject(editor);
            receiver->closeStack();
        }
        {
            const auto receiver = compat::ngRequireObject(editor);
            receiver->dispose();
        }
        editor.reset();
    }
}
void DepthEditViewport::selectionChanged(compat::Slice<std::shared_ptr<Node>> nodes) {
    const auto keepAlive = weak_from_this().lock();
    selection = nodes;
    syncTargets();
}
std::shared_ptr<tools::base::DepthEditTool> DepthEditViewport::activeTool() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    return tools::ngDepthEditTool(toolMode);
}
void DepthEditViewport::setToolMode(DepthToolMode mode) {
    const auto keepAlive = weak_from_this().lock();
    toolMode = mode;
    if (const auto tool = activeTool()) {
        tool->selected(keepAlive ? keepAlive : shared_from_this());
    }
}
void DepthEditViewport::drawDepthOperationOptions() {
    const auto keepAlive = weak_from_this().lock();
    if (!editor) return;
    if (widgets::ngBeginDropdownMenuRetained(compat::RetainedString::fromStaticLiteral("DEPTH_DEFORMERS"),
        compat::RetainedString::fromStaticLiteral("Depth"), ImVec2(280, 0),
        ImVec2(360, std::numeric_limits<float>::max()))) {
        {
            const auto receiver = compat::ngRequireObject(editor);
            receiver->drawOperationOptions();
        }
        widgets::ngEndDropdownMenu();
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Depth Deformers"))));
}
void DepthEditViewport::draw(std::shared_ptr<Camera> camera) {
    const auto keepAlive = shared_from_this();
    compat::ngRequireObject(project::incActivePuppet())->update();
    if (editor) {
        const auto receiver = compat::ngRequireObject(editor);
        receiver->draw(camera, depthCamera);
    }
    if (const auto tool = activeTool()) tool->draw(camera, keepAlive);
}
void DepthEditViewport::drawTools() {
    const auto keepAlive = weak_from_this().lock();
    ImGui::SetWindowFontScale(1.30f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 10));
    for (const auto tool : tools::ngDepthEditTools()) {
        if (compat::ngRequireObject(tool)->drawToolButton(toolMode)) setToolMode(tool->mode());
    }
    ImGui::PopStyleVar(2);
    ImGui::SetWindowFontScale(1);
}
void DepthEditViewport::drawOptions() {
    const auto keepAlive = weak_from_this().lock();
    drawDepthOptions();
    if (const auto tool = activeTool()) {
        ImGui::SameLine(0, 6);
        tool->drawOptions(keepAlive ? keepAlive : shared_from_this());
    }
    ImGui::SameLine(0, 6);
    drawDepthOperationOptions();
}
void DepthEditViewport::drawConfirmBar() {
    const auto keepAlive = weak_from_this().lock();
    if (!editor) return;
    const auto targets = [&] {
        const auto receiver = compat::ngRequireObject(editor);
        return receiver->getTargets();
    }();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 4));
    if (widgets::ngButtonColored(core::translation::ngTranslateCString("\uE668 Apply").data(), ImVec2(0, 26))) {
        {
            const auto receiver = compat::ngRequireObject(editor);
            receiver->closeStack();
        }
        {
            const auto receiver = compat::ngRequireObject(editor);
            receiver->applyToTargets();
        }
        leaveToModel(targets);
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Apply"))));
    ImGui::SameLine(0, 0);
    if (widgets::ngButtonColored(core::translation::ngTranslateCString("\uE5CD Cancel").data(), ImVec2(0, 26))) {
        if (ImGui::GetIO().KeyShift) {
            const auto receiver = compat::ngRequireObject(editor);
            receiver->resetFromTargets();
        }
        {
            const auto receiver = compat::ngRequireObject(editor);
            receiver->closeStack();
        }
        leaveToModel(targets);
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Cancel"))));
    ImGui::PopStyleVar();
}
void DepthEditViewport::update(ImGuiIO* io, std::shared_ptr<Camera> camera) {
    const auto keepAlive = shared_from_this();
    bool consumed = false;
    if (const auto tool = activeTool()) consumed = tool->update(io, camera, keepAlive);
    if (!consumed) updateDepthCamera(io);
    if (editor) {
        const auto receiver = compat::ngRequireObject(editor);
        receiver->update(io, camera);
    }
}
void DepthEditViewport::updatePresentation(ImGuiIO* io, std::shared_ptr<Camera>) {
    const auto keepAlive = weak_from_this().lock();
    updateDepthCamera(io);
}
}
