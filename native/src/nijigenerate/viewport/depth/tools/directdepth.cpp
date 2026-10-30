// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
// Used inmath vector dot: David Herberth and Inochi2D Project, MIT.
// Permission/disclaimer: port/licenses/inmath-depthbone-quaternion.txt.
#include "directdepth.hpp"
#include "operation.hpp"
#include "../viewport.hpp"
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <imgui.h>
#include <algorithm>

namespace nijigenerate::viewport::depth::tools::directdepth {
namespace {
[[maybe_unused]] const bool directDepthSourceClassInfo = [] {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthDirectDepthTool),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.viewport.depth.tools.directdepth.DepthDirectDepthTool"));
    return true;
}();
}
float DepthDirectDepthTool::visibleDepthDelta(Vec2 currentView) {
    // inmath dot accumulates in the vector element type, starting with zero.
    float denom = 0; denom += depthAxisView.x * depthAxisView.x; denom += depthAxisView.y * depthAxisView.y;
    if (denom > .000001f) {
        const Vec2 delta{currentView.x - dragOriginView.x, currentView.y - dragOriginView.y};
        float numerator = 0; numerator += delta.x * depthAxisView.x; numerator += delta.y * depthAxisView.y;
        return numerator / denom;
    }
    return -(currentView.y - dragOriginView.y) * .006f;
}
void DepthDirectDepthTool::applyClickSelection(ImGuiIO* io, Editor editor, std::ptrdiff_t index) {
    maybeSelectOneEditor.reset(); maybeSelectOneVertex = -1;
    if (compat::ngRequireObject(io)->KeyCtrl && !compat::ngRequireObject(io)->KeyShift)
        compat::ngRequireObject(editor)->deselectVertex(index);
    else if (compat::ngRequireObject(io)->KeyShift) compat::ngRequireObject(editor)->toggleVertexSelection(index);
    else if (!compat::ngRequireObject(editor)->isVertexSelected(index))
        compat::ngRequireObject(editor)->selectOneVertex(index);
    else { maybeSelectOneEditor = editor; maybeSelectOneVertex = index; }
}
bool DepthDirectDepthTool::hitVertex(ImGuiIO* io, Camera camera, Viewport viewport,
        Editor& editor, std::ptrdiff_t& index) {
    editor.reset();
    index = 0;
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
    if (!editorSet) { editor.reset(); index = -1; return false; }
    editor = nearestVertexFromScreenMouse(io, camera, viewport,
        14.0f / std::max(.01f, nijigenerate::viewport::base::viewportZoom), index);
    return editor && index >= 0;
}
bool DepthDirectDepthTool::beginPending(ImGuiIO* io, Camera camera, Viewport viewport) {
    Editor editor; std::ptrdiff_t index = 0; const auto mouse = screenMouseToView(io, camera);
    if (!hitVertex(io, camera, viewport, editor, index)) {
        selecting = true; selectAdd = compat::ngRequireObject(io)->KeyShift;
        selectRemove = compat::ngRequireObject(io)->KeyCtrl && !compat::ngRequireObject(io)->KeyShift;
        selectOriginView = mouse; selectCurrentView = mouse;
        maybeSelectOneEditor.reset(); maybeSelectOneVertex = -1; return true;
    }
    applyClickSelection(io, editor, index);
    if (!compat::ngRequireObject(editor)->isVertexSelected(index)) return true;
    pendingEditor = editor; pendingVertex = index; dragOriginView = mouse; return true;
}
bool DepthDirectDepthTool::beginDepthDrag(Viewport viewport) {
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
    if (!editorSet || !pendingEditor || pendingVertex < 0) return false;
    Indices selection;
    {
        const auto receiver = pendingEditor;
        selection = compat::ngRequireObject(receiver)->selectedVertexIndices();
    }
    if (selection.empty()) {
        {
            const auto receiver = pendingEditor;
            compat::ngRequireObject(receiver)->selectOneVertex(pendingVertex);
        }
        {
            const auto receiver = pendingEditor;
            selection = compat::ngRequireObject(receiver)->selectedVertexIndices();
        }
    }
    draggedVertices = selection; activeEditor = pendingEditor; activeVertex = pendingVertex;
    pendingEditor.reset(); pendingVertex = -1; maybeSelectOneEditor.reset(); maybeSelectOneVertex = -1;
    action = mesheditor::action::DepthEditorDepthChangeAction::createSource([&] {
        return std::tuple{editorSet, activeEditor};
    });
    editorSet->beginDirectDepthEdit(activeEditor);
    {
        const auto receiver = activeEditor;
        startDepth = compat::ngRequireObject(receiver)->getDepth(std::size_t(activeVertex));
    }
    {
        const auto receiver = activeEditor;
        startDepths = compat::ngRequireObject(receiver)->copyEditorDepths();
    }
    Vec2 local;
    {
        const auto receiver = activeEditor;
        local = compat::ngRequireObject(receiver)->localVertex(std::size_t(activeVertex));
    }
    Vec2 p0, p1;
    {
        // Keep each evaluated receiver and depth value across the later
        // virtual viewport call, then re-read source state for the next call.
        const auto receiver = compat::ngRequireObject(activeEditor);
        const auto depth = startDepth;
        auto& camera = compat::ngRequireObject(viewport)->depthCameraState();
        p0 = receiver->projectLocalPoint(local, depth, camera);
    }
    {
        const auto receiver = compat::ngRequireObject(activeEditor);
        const auto depth = startDepth + 1.0f;
        auto& camera = compat::ngRequireObject(viewport)->depthCameraState();
        p1 = receiver->projectLocalPoint(local, depth, camera);
    }
    depthAxisView = {p1.x - p0.x, p1.y - p0.y}; return true;
}
void DepthDirectDepthTool::updateDrag(ImGuiIO* io, Camera camera, Viewport viewport) {
    if (!activeEditor || activeVertex < 0) return;
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor(); if (!editorSet) return;
    {
        const auto receiver = activeEditor;
        compat::ngRequireObject(receiver)->replaceEditorDepths(startDepths);
    }
    const auto delta = visibleDepthDelta(screenMouseToView(io, camera));
    const auto vertices = draggedVertices;
    for (auto index : vertices) {
        if (index < 0 || std::size_t(index) >= startDepths.size()) continue;
        const auto receiver = activeEditor;
        compat::ngRequireObject(receiver)->setDepth(std::size_t(index), startDepths[std::size_t(index)] + delta);
    }
    editorSet->markDirectDepthDirty(activeEditor);
}
void DepthDirectDepthTool::endDrag() {
    if (action) {
        {
            const auto receiver = action;
            compat::ngRequireObject(receiver)->updateNewState();
        }
        core::actionstack::incActionPush(action);
    }
    activeEditor.reset(); activeVertex = -1; startDepths = {}; draggedVertices = {}; action.reset();
}
void DepthDirectDepthTool::updateSelection(ImGuiIO* io, Camera camera, Viewport) {
    selectCurrentView = screenMouseToView(io, camera);
}
void DepthDirectDepthTool::endSelection(Viewport viewport) {
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor(); if (!editorSet) return;
    const auto editors = editorSet->getEditors();
    for (auto editor : editors) {
        const auto indices = compat::ngRequireObject(editor)->projectedVerticesInRect(selectOriginView, selectCurrentView);
        if (selectRemove) for (auto index : indices) compat::ngRequireObject(editor)->deselectVertex(index);
        else if (selectAdd) for (auto index : indices) compat::ngRequireObject(editor)->addVertexSelection(index);
        else {
            compat::ngRequireObject(editor)->clearVertexSelection();
            for (auto index : indices) compat::ngRequireObject(editor)->addVertexSelection(index);
        }
    }
    selecting = false;
}
void DepthDirectDepthTool::commitMaybeSelectOne() {
    if (maybeSelectOneEditor && maybeSelectOneVertex >= 0) {
        const auto receiver = maybeSelectOneEditor;
        compat::ngRequireObject(receiver)->selectOneVertex(maybeSelectOneVertex);
    }
    maybeSelectOneEditor.reset(); maybeSelectOneVertex = -1;
}
void DepthDirectDepthTool::clearPending() { pendingEditor.reset(); pendingVertex = -1; }
void DepthDirectDepthTool::drawRect(Vec2 p0, Vec2 p1) {
    core::dbg::inDbgSetBuffer(nijigenerate::viewport::common::ngCreateRectBuffer(p0, p1));
    if (!selectAdd && !selectRemove) core::dbg::inDbgDrawLines({1, 0, 0, 1});
    else if (selectRemove) core::dbg::inDbgDrawLines({0, 1, 1, .8f});
    else core::dbg::inDbgDrawLines({0, 1, 0, .8f});
}
void DepthDirectDepthTool::drawEditorSelection(Editor editor, std::ptrdiff_t hotIndex, Indices candidates) {
    using nicxlive::core::math::Vec4;
    const auto selectedReceiver = compat::ngRequireObject(editor);
    const auto selectedIndices = selectedReceiver->selectedVertexIndices();
    for (auto index : selectedIndices) {
        if (index < 0 || std::size_t(index) >= editor->projectedPoints.size()) continue;
        const auto color = selecting && !selectAdd && !selectRemove ? Vec4{.6f, 0, 0, 1} : Vec4{1, 0, 0, 1};
        operation::drawDepthPoint(editor->projectedPoints[std::size_t(index)], color, 6);
    }
    for (auto index : candidates) {
        if (index < 0 || std::size_t(index) >= editor->projectedPoints.size()) continue;
        const auto color = selectRemove ? Vec4{1, 0, 1, 1} : Vec4{1, 0, 0, 1};
        operation::drawDepthPoint(editor->projectedPoints[std::size_t(index)], color, 6);
    }
    if (hotIndex >= 0 && std::size_t(hotIndex) < editor->projectedPoints.size())
        operation::drawDepthPoint(editor->projectedPoints[std::size_t(hotIndex)], {1, 1, 1, .3f}, 15);
}
DepthToolMode DepthDirectDepthTool::mode() { return DepthToolMode::DirectDepth; }
const char* DepthDirectDepthTool::icon() { return "\xEF\x9C\x9F"; }
compat::RetainedString DepthDirectDepthTool::tooltip() {
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Adjust Vertex Depth"));
}
void DepthDirectDepthTool::draw(Camera camera, Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    const auto viewportReceiver = compat::ngRequireObject(viewport);
    const auto editorSet = viewportReceiver->getEditor();
    if (!editorSet) return;
    std::ptrdiff_t hotIndex = -1; Editor hotEditor;
    if (!selecting && !activeEditor) hitVertex(&ImGui::GetIO(), camera, viewport, hotEditor, hotIndex);
    const auto editors = editorSet->getEditors();
    for (auto editor : editors) {
        const auto candidates = selecting
            ? compat::ngRequireObject(editor)->projectedVerticesInRect(selectOriginView, selectCurrentView)
            : Indices{};
        drawEditorSelection(editor, editor == hotEditor ? hotIndex : -1, candidates);
    }
    if (selecting) drawRect(selectOriginView, selectCurrentView);
}
bool DepthDirectDepthTool::update(ImGuiIO* io, Camera camera, Viewport viewport) {
    const auto keepAlive = weak_from_this().lock();
    if (activeEditor) {
        if (compat::ngRequireObject(io)->MouseDown[0]) { updateDrag(io, camera, viewport); return true; }
        endDrag(); return true;
    }
    if (selecting) {
        if (compat::ngRequireObject(io)->MouseDown[0]) { updateSelection(io, camera, viewport); return true; }
        endSelection(viewport); return true;
    }
    if (pendingEditor) {
        if (compat::ngRequireObject(io)->MouseDown[0]) {
            if (core::input::ngInputIsDragRequested(ImGuiMouseButton_Left)) return beginDepthDrag(viewport);
            return true;
        }
        commitMaybeSelectOne(); clearPending(); return true;
    }
    if (core::input::ngInputIsMouseClicked(ImGuiMouseButton_Left)) return beginPending(io, camera, viewport);
    return false;
}
}
