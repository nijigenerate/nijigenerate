/*
    Depth plane flattening tool.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.plane;

import bindbc.imgui;
import i18n;
import nijigenerate.commands : Context, cmd;
import nijigenerate.commands.depth.editor : DepthEditorOperationCommand;
import nijigenerate.core.input;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.mesheditor;
import nijigenerate.viewport.depth.tools.base;
import nijigenerate.viewport.depth.tools.operation;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijilive;
import std.algorithm : max;
import std.math : abs;

class DepthPlaneTool : DepthEditTool {
private:
    DepthMeshEditorOne activeEditor;
    ptrdiff_t centerVertex = -1;
    vec2 centerPoint;
    float radiusX;
    float radiusY;

    bool nearest(ImGuiIO* io, Camera camera, DepthEditViewport viewport, out DepthMeshEditorOne editor, out ptrdiff_t index) {
        editor = nearestVertexFromScreenMouse(io, camera, viewport, index);
        return editor !is null && index >= 0;
    }

    vec2 center() {
        return centerPoint;
    }

    vec2 localMouse(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        return screenMouseToModel(io, camera, activeEditor, viewport);
    }

    void updateRadius(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        DepthMeshEditorOne editor;
        ptrdiff_t index;
        if (!nearest(io, camera, viewport, editor, index) || editor !is activeEditor) return;
        auto c = center();
        auto p = viewport.brushSettings().snapToGrid
            ? activeEditor.localVertex(cast(size_t)index)
            : localMouse(io, camera, viewport);
        radiusX = max(4.0f, abs(p.x - c.x));
        radiusY = max(4.0f, abs(p.y - c.y));
    }

    void apply(DepthEditViewport viewport) {
        if (activeEditor is null || centerVertex < 0) return;
        auto editorSet = viewport.getEditor();
        auto ctx = new Context();
        cmd!(DepthEditorOperationCommand.AddEditorDepthOp)(
            ctx, editorSet, activeEditor, new DepthPlaneOperation(center(), radiusX, radiusY, viewport.brushSettings()), -1);
    }

public:
    override DepthToolMode mode() { return DepthToolMode.Plane; }
    override const(char)* icon() { return "\uF507"; } // collapse_content
    override string tooltip() { return _("Flatten Depth Plane"); }

    override
    void draw(Camera camera, DepthEditViewport viewport) {
        if (activeEditor is null || centerVertex < 0) return;
        drawDepthEllipse(activeEditor, center(), radiusX, radiusY, viewport.brushSettings().angle, viewport.brushSettings().amount, viewport.depthCameraState(), depthOperationColor(viewport.brushSettings().amount, true));
    }

    override
    void drawOptions(DepthEditViewport viewport) {
        auto settings = &viewport.brushSettings();
        drawOptionDrag("Target", &settings.amount, 0.01f, -2.0f, 2.0f, "%.2f");
        drawOptionDrag("Radius X", &settings.radiusX, 1.0f, 1.0f, 1000.0f, "%.0f");
        drawOptionDrag("Radius Y", &settings.radiusY, 1.0f, 1.0f, 1000.0f, "%.0f");
        drawOptionDrag("Angle", &settings.angle, 1.0f, -180.0f, 180.0f, "%.0f");
        drawOptionDrag("Flatten", &settings.flattenStrength, 0.01f, 0.0f, 1.0f, "%.2f", false);
    }

    override
    bool update(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        if (activeEditor !is null) {
            if (io.MouseDown[0]) {
                updateRadius(io, camera, viewport);
                return true;
            }
            apply(viewport);
            activeEditor = null;
            centerVertex = -1;
            return true;
        }

        if (!incInputIsMouseClicked(ImGuiMouseButton.Left)) return false;
        ptrdiff_t index;
        if (!nearest(io, camera, viewport, activeEditor, index)) return false;
        centerVertex = index;
        centerPoint = viewport.brushSettings().snapToGrid
            ? activeEditor.localVertex(cast(size_t)index)
            : localMouse(io, camera, viewport);
        radiusX = viewport.brushSettings().radiusX;
        radiusY = viewport.brushSettings().radiusY;
        return true;
    }
}
