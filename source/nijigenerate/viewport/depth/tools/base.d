/*
    Depth edit tool base.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.base;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.core.input;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.mesheditor.editor;
import nijigenerate.viewport.depth.mesheditor.node;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijigenerate.widgets.button;
import nijigenerate.widgets.drag;
import nijigenerate.widgets.tooltip;
import nijilive;
import std.algorithm : max;

abstract class DepthEditTool {
    abstract DepthToolMode mode();
    abstract const(char)* icon();
    abstract string tooltip();

    void selected(DepthEditViewport viewport) { }
    void draw(Camera camera, DepthEditViewport viewport) { }
    void drawOptions(DepthEditViewport viewport) { }
    bool update(ImGuiIO* io, Camera camera, DepthEditViewport viewport) { return false; }

    vec2 screenMouseToView(ImGuiIO* io, Camera camera) {
        // MainViewport.poll already converts the current ImGui screen mouse
        // position into the normal viewport world coordinate with the active
        // viewport camera. Existing mesh tools invert that value before
        // comparing it with points drawn through inDbg/camera.matrix().
        return -incInputGetMousePosition();
    }

    vec2 modelToView(DepthMeshEditorOne editor, vec2 point, float depth, DepthEditViewport viewport) {
        return editor.modelToDepthView(point, depth, viewport.depthCameraState());
    }

    vec2 viewToModel(DepthMeshEditorOne editor, vec2 point, DepthEditViewport viewport, float depth = 0.0f) {
        return editor.depthViewToModel(point, viewport.depthCameraState(), depth);
    }

    vec2 screenMouseToModel(ImGuiIO* io, Camera camera, DepthMeshEditorOne editor, DepthEditViewport viewport, float depth = 0.0f) {
        return viewToModel(editor, screenMouseToView(io, camera), viewport, depth);
    }

    DepthMeshEditorOne nearestVertexFromScreenMouse(ImGuiIO* io, Camera camera, DepthEditViewport viewport, float radius, out ptrdiff_t index) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return null;
        return editorSet.findNearestProjectedVertex(screenMouseToView(io, camera), radius, index);
    }

    DepthMeshEditorOne nearestVertexFromScreenMouse(ImGuiIO* io, Camera camera, DepthEditViewport viewport, out ptrdiff_t index) {
        return nearestVertexFromScreenMouse(io, camera, viewport, float.max, index);
    }

    protected bool drawOptionDrag(
        string id,
        float* value,
        float adjustSpeed,
        float minValue,
        float maxValue,
        string fmt,
        bool sameLineAfter = true
    ) {
        igPushID(id.ptr, id.ptr + id.length);
        igPushItemWidth(72);
        auto changed = incDragFloat(id, value, adjustSpeed, minValue, maxValue, fmt, ImGuiSliderFlags.NoRoundToFormat);
        igPopItemWidth();
        igPopID();
        incTooltip(id);
        if (sameLineAfter) igSameLine(0, 4);
        return changed;
    }

    bool drawToolButton(DepthToolMode activeMode) {
        if (incButtonColored(icon(), ImVec2(0, 0), activeMode == mode ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
            return true;
        }
        incTooltip(tooltip());
        return false;
    }
}
