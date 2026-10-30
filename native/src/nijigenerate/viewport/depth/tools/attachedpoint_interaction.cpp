// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "attachedpoint.hpp"
#include "../viewport.hpp"
#include "../mesheditor/editor.hpp"
#include <nijigenerate/commands/depth/editor_commands.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_allocator.hpp>
#include <imgui.h>

namespace nijigenerate::viewport::depth::tools::attachedpoint {
bool DepthAttachedPointTool::beginDrag(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport) {
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
    if (!editorSet) return false;
    std::ptrdiff_t index = 0;
    const auto editor = nearestVertexFromScreenMouse(io, camera, viewport, index);
    if (!editor || index < 0) return false;
    activeEditor = editor;
    activeVertex = index;
    dragOriginY = compat::ngRequireObject(io)->MousePos.y;
    operation = operation::DepthAttachedPointOperation::createSource([&] {
        return std::tuple{std::size_t(index), 0};
    });
    editorSet->appendOperation(editor, operation);
    editor->selectVertex(index);
    return true;
}
void DepthAttachedPointTool::updateDrag(ImGuiIO* io, std::shared_ptr<viewport::DepthEditViewport> viewport) {
    if (!activeEditor || activeVertex < 0 || !operation) return;
    operation->amount = -(compat::ngRequireObject(io)->MousePos.y - dragOriginY) * 0.006f;
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
    compat::ngRequireObject(editorSet)->recompute(activeEditor);
}
void DepthAttachedPointTool::endDrag(std::shared_ptr<viewport::DepthEditViewport> viewport) {
    if (operation && activeEditor) {
        auto context = std::allocate_shared<commands::Context>(
            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        // Preserve source argument order across the virtual viewport callback.
        const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
        const auto target = activeEditor;
        const auto pendingOperation = operation;
        commands::depth::editor::ngRunDepthEditorCommand<
            commands::depth::editor::DepthEditorOperationCommand::AddEditorDepthOp>(
                context, editorSet, target, pendingOperation, -1);
    }
    activeEditor.reset(); activeVertex = -1; operation.reset();
}
bool DepthAttachedPointTool::update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport) {
    const auto keepAlive = weak_from_this().lock();
    if (activeEditor) {
        if (compat::ngRequireObject(io)->MouseDown[0]) { updateDrag(io, viewport); return true; }
        endDrag(viewport); return true;
    }
    if (core::input::ngInputIsMouseClicked(ImGuiMouseButton_Left)) return beginDrag(io, camera, viewport);
    return false;
}
}
