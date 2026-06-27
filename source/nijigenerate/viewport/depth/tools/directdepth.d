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
import nijigenerate.core.input;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.mesheditor;
import nijigenerate.viewport.depth.tools.base;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijilive;
import std.algorithm : max;

class DepthDirectDepthTool : DepthEditTool {
private:
    DepthMeshEditorOne activeEditor;
    ptrdiff_t activeVertex = -1;
    vec2 dragOriginView;
    vec2 depthAxisView;
    float startDepth;
    float[] startDepths;
    DepthEditorDepthChangeAction action;

    float visibleDepthDelta(vec2 currentView) {
        auto denom = depthAxisView.dot(depthAxisView);
        if (denom > 0.000001f) {
            return (currentView - dragOriginView).dot(depthAxisView) / denom;
        }
        return -(currentView.y - dragOriginView.y) * 0.006f;
    }

    bool beginDrag(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return false;

        ptrdiff_t index;
        auto editor = nearestVertexFromScreenMouse(io, camera, viewport, 14.0f / max(0.01f, incViewportZoom), index);
        if (editor is null || index < 0) return false;

        activeEditor = editor;
        activeVertex = index;
        dragOriginView = screenMouseToView(io, camera);
        action = new DepthEditorDepthChangeAction(editorSet, editor);
        editorSet.beginDirectDepthEdit(editor);

        startDepth = editor.getDepth(cast(size_t)index);
        startDepths = editor.copyEditorDepths();

        auto local = editor.localVertex(cast(size_t)index);
        auto p0 = editor.projectLocalPoint(local, startDepth, viewport.depthCameraState());
        auto p1 = editor.projectLocalPoint(local, startDepth + 1.0f, viewport.depthCameraState());
        depthAxisView = p1 - p0;

        editor.selectVertex(index);
        return true;
    }

    void updateDrag(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        if (activeEditor is null || activeVertex < 0) return;
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return;

        activeEditor.replaceEditorDepths(startDepths);
        activeEditor.setDepth(cast(size_t)activeVertex, startDepth + visibleDepthDelta(screenMouseToView(io, camera)));
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
        action = null;
    }

public:
    override DepthToolMode mode() { return DepthToolMode.DirectDepth; }
    override const(char)* icon() { return __("Z+"); }
    override string tooltip() { return _("Adjust Vertex Depth"); }

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

        if (incInputIsMouseClicked(ImGuiMouseButton.Left)) {
            return beginDrag(io, camera, viewport);
        }
        return false;
    }
}
