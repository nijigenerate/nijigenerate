/*
    Direct vertex depth editing tool.

    Copyright ﾂｩ 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.directdepth;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.core.dbg;
import nijigenerate.core.input;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.mesheditor;
import nijigenerate.viewport.depth.tools.base;
import nijigenerate.viewport.depth.tools.operation;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijilive;
import std.algorithm : max;

class DepthDirectDepthTool : DepthEditTool {
private:
    DepthMeshEditorOne activeEditor;
    ptrdiff_t activeVertex = -1;
    DepthMeshEditorOne pendingEditor;
    ptrdiff_t pendingVertex = -1;
    DepthMeshEditorOne maybeSelectOneEditor;
    ptrdiff_t maybeSelectOneVertex = -1;
    bool selecting;
    bool selectAdd;
    bool selectRemove;
    vec2 selectOriginView;
    vec2 selectCurrentView;
    vec2 dragOriginView;
    vec2 depthAxisView;
    float startDepth;
    float[] startDepths;
    ptrdiff_t[] draggedVertices;
    DepthEditorDepthChangeAction action;

    float visibleDepthDelta(vec2 currentView) {
        auto denom = depthAxisView.dot(depthAxisView);
        if (denom > 0.000001f) {
            return (currentView - dragOriginView).dot(depthAxisView) / denom;
        }
        return -(currentView.y - dragOriginView.y) * 0.006f;
    }

    void applyClickSelection(ImGuiIO* io, DepthMeshEditorOne editor, ptrdiff_t index) {
        maybeSelectOneEditor = null;
        maybeSelectOneVertex = -1;
        if (io.KeyCtrl && !io.KeyShift) {
            editor.deselectVertex(index);
        } else if (io.KeyShift) {
            editor.toggleVertexSelection(index);
        } else if (!editor.isVertexSelected(index)) {
            editor.selectOneVertex(index);
        } else {
            maybeSelectOneEditor = editor;
            maybeSelectOneVertex = index;
        }
    }

    bool hitVertex(ImGuiIO* io, Camera camera, DepthEditViewport viewport, out DepthMeshEditorOne editor, out ptrdiff_t index) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null) {
            editor = null;
            index = -1;
            return false;
        }

        editor = nearestVertexFromScreenMouse(io, camera, viewport, 14.0f / max(0.01f, incViewportZoom), index);
        return editor !is null && index >= 0;
    }

    bool beginPending(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        DepthMeshEditorOne editor;
        ptrdiff_t index;
        auto mouse = screenMouseToView(io, camera);
        if (!hitVertex(io, camera, viewport, editor, index)) {
            selecting = true;
            selectAdd = io.KeyShift;
            selectRemove = io.KeyCtrl && !io.KeyShift;
            selectOriginView = mouse;
            selectCurrentView = mouse;
            maybeSelectOneEditor = null;
            maybeSelectOneVertex = -1;
            return true;
        }

        applyClickSelection(io, editor, index);
        if (!editor.isVertexSelected(index)) return true;
        pendingEditor = editor;
        pendingVertex = index;
        dragOriginView = mouse;
        return true;
    }

    bool beginDepthDrag(DepthEditViewport viewport) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null || pendingEditor is null || pendingVertex < 0) return false;

        auto selection = pendingEditor.selectedVertexIndices();
        if (selection.length == 0) {
            pendingEditor.selectOneVertex(pendingVertex);
            selection = pendingEditor.selectedVertexIndices();
        }

        draggedVertices = selection;
        activeEditor = pendingEditor;
        activeVertex = pendingVertex;
        pendingEditor = null;
        pendingVertex = -1;
        maybeSelectOneEditor = null;
        maybeSelectOneVertex = -1;
        action = new DepthEditorDepthChangeAction(editorSet, activeEditor);
        editorSet.beginDirectDepthEdit(activeEditor);

        startDepth = activeEditor.getDepth(cast(size_t)activeVertex);
        startDepths = activeEditor.copyEditorDepths();

        auto local = activeEditor.localVertex(cast(size_t)activeVertex);
        auto p0 = activeEditor.projectLocalPoint(local, startDepth, viewport.depthCameraState());
        auto p1 = activeEditor.projectLocalPoint(local, startDepth + 1.0f, viewport.depthCameraState());
        depthAxisView = p1 - p0;
        return true;
    }

    void updateDrag(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        if (activeEditor is null || activeVertex < 0) return;
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return;

        activeEditor.replaceEditorDepths(startDepths);
        auto delta = visibleDepthDelta(screenMouseToView(io, camera));
        foreach (index; draggedVertices) {
            if (index < 0 || index >= startDepths.length) continue;
            activeEditor.setDepth(cast(size_t)index, startDepths[index] + delta);
        }
        editorSet.markDirectDepthDirty(activeEditor);
    }

    void endDrag() {
        if (action !is null) {
            action.updateNewState();
            incActionPush(action);
        }
        activeEditor = null;
        activeVertex = -1;
        startDepths = null;
        draggedVertices = null;
        action = null;
    }

    void updateSelection(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        selectCurrentView = screenMouseToView(io, camera);
    }

    void endSelection(DepthEditViewport viewport) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return;
        foreach (editor; editorSet.getEditors()) {
            auto indices = editor.projectedVerticesInRect(selectOriginView, selectCurrentView);
            if (selectRemove) {
                foreach (index; indices) editor.deselectVertex(index);
            } else if (selectAdd) {
                foreach (index; indices) editor.addVertexSelection(index);
            } else {
                editor.clearVertexSelection();
                foreach (index; indices) editor.addVertexSelection(index);
            }
        }
        selecting = false;
    }

    void commitMaybeSelectOne() {
        if (maybeSelectOneEditor !is null && maybeSelectOneVertex >= 0) {
            maybeSelectOneEditor.selectOneVertex(maybeSelectOneVertex);
        }
        maybeSelectOneEditor = null;
        maybeSelectOneVertex = -1;
    }

    void clearPending() {
        pendingEditor = null;
        pendingVertex = -1;
    }

    void drawRect(vec2 p0, vec2 p1) {
        inDbgSetBuffer(incCreateRectBuffer(p0, p1));
        if (!selectAdd && !selectRemove) {
            inDbgDrawLines(vec4(1, 0, 0, 1));
        } else if (selectRemove) {
            inDbgDrawLines(vec4(0, 1, 1, 0.8f));
        } else {
            inDbgDrawLines(vec4(0, 1, 0, 0.8f));
        }
    }

    void drawEditorSelection(DepthMeshEditorOne editor, ptrdiff_t hotIndex, ptrdiff_t[] candidateIndices = null) {
        if (editor.projectedPoints.length > 0) {
            Vec3Array points;
            foreach (point; editor.projectedPoints) {
                points ~= vec3(point, 0);
            }
            inDbgSetBuffer(points);
            inDbgPointsSize(10);
            inDbgDrawPoints(vec4(0, 0, 0, 1));
            inDbgPointsSize(6);
            inDbgDrawPoints(vec4(1, 1, 1, 1));
        }
        foreach (index; editor.selectedVertexIndices()) {
            if (index < 0 || index >= editor.projectedPoints.length) continue;
            auto color = selecting && !selectAdd && !selectRemove ? vec4(0.6f, 0, 0, 1) : vec4(1, 0, 0, 1);
            drawDepthPoint(editor.projectedPoints[index], color, 6);
        }
        foreach (index; candidateIndices) {
            if (index < 0 || index >= editor.projectedPoints.length) continue;
            auto color = selectRemove ? vec4(1, 0, 1, 1) : vec4(1, 0, 0, 1);
            drawDepthPoint(editor.projectedPoints[index], color, 6);
        }
        if (hotIndex >= 0 && hotIndex < editor.projectedPoints.length) {
            drawDepthPoint(editor.projectedPoints[hotIndex], vec4(1, 1, 1, 0.3f), 15);
        }
    }

public:
    override DepthToolMode mode() { return DepthToolMode.DirectDepth; }
    override const(char)* icon() { return "\uE8D5"; }
    override string tooltip() { return _("Adjust Vertex Depth"); }

    override
    void draw(Camera camera, DepthEditViewport viewport) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return;

        ptrdiff_t hotIndex = -1;
        DepthMeshEditorOne hotEditor;
        if (!selecting && activeEditor is null) {
            auto io = igGetIO();
            hitVertex(io, camera, viewport, hotEditor, hotIndex);
        }

        foreach (editor; editorSet.getEditors()) {
            auto candidates = selecting ? editor.projectedVerticesInRect(selectOriginView, selectCurrentView) : null;
            drawEditorSelection(editor, editor is hotEditor ? hotIndex : -1, candidates);
        }
        if (selecting) {
            drawRect(selectOriginView, selectCurrentView);
        }
    }

    override
    bool update(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        if (activeEditor !is null) {
            if (io.MouseDown[0]) {
                updateDrag(io, camera, viewport);
                return true;
            }
            endDrag();
            return true;
        }

        if (selecting) {
            if (io.MouseDown[0]) {
                updateSelection(io, camera, viewport);
                return true;
            }
            endSelection(viewport);
            return true;
        }

        if (pendingEditor !is null) {
            if (io.MouseDown[0]) {
                if (incInputIsDragRequested(ImGuiMouseButton.Left)) {
                    return beginDepthDrag(viewport);
                }
                return true;
            }
            commitMaybeSelectOne();
            clearPending();
            return true;
        }

        if (incInputIsMouseClicked(ImGuiMouseButton.Left)) {
            return beginPending(io, camera, viewport);
        }
        return false;
    }
}
