/*
    Depth ring deformation tool.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.ring;

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
import nijigenerate.widgets.button;
import nijigenerate.widgets.tooltip;
import nijilive;
import std.algorithm : max;
import std.math : cos, sin;

class DepthRingTool : DepthEditTool {
private:
    enum RingToolMode {
        Create,
        Edit,
    }

    DepthMeshEditorOne activeEditor;
    ptrdiff_t startVertex = -1;
    ptrdiff_t currentVertex = -1;
    vec2 startPoint;
    vec2 currentPoint;
    RingToolMode toolMode = RingToolMode.Create;

    bool isRingOperation(DepthOperation operation) {
        return cast(DepthRingOperation)operation !is null;
    }

    void resetCreateDrag() {
        activeEditor = null;
        startVertex = -1;
        currentVertex = -1;
    }

    void setMode(RingToolMode mode) {
        if (toolMode == mode) return;
        toolMode = mode;
        resetCreateDrag();
    }

    void toggleMode() {
        setMode(toolMode == RingToolMode.Create ? RingToolMode.Edit : RingToolMode.Create);
    }

    bool nearest(ImGuiIO* io, Camera camera, DepthEditViewport viewport, out DepthMeshEditorOne editor, out ptrdiff_t index) {
        editor = nearestVertexFromScreenMouse(io, camera, viewport, index);
        return editor !is null && index >= 0;
    }

    vec2 localMouse(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        return screenMouseToModel(io, camera, activeEditor, viewport);
    }

    DepthRingOperation buildOperation(DepthEditViewport viewport) {
        if (activeEditor is null || startVertex < 0) return null;
        auto p0 = startPoint;
        auto p1 = currentPoint;
        if ((p1 - p0).length() < 1.0f) {
            auto settings = viewport.brushSettings();
            auto angle = settings.angle * 3.14159265358979323846f / 180.0f;
            auto d = vec2(cos(angle), sin(angle)) * settings.radiusX;
            p0 -= d;
            p1 += d;
        }
        return new DepthRingOperation(p0, p1, viewport.brushSettings());
    }

public:
    override DepthToolMode mode() { return DepthToolMode.Ring; }
    override const(char)* icon() { return "\uF71A"; } // input_circle
    override string tooltip() { return _("Edit Depth Ring"); }

    override
    void draw(Camera camera, DepthEditViewport viewport) {
        if (activeEditor is null || startVertex < 0) return;
        auto p0 = startPoint;
        auto p1 = currentPoint;
        if ((p1 - p0).length() < 1.0f) {
            auto settings = viewport.brushSettings();
            auto angle = settings.angle * 3.14159265358979323846f / 180.0f;
            p1 = p0 + vec2(cos(angle), sin(angle)) * settings.radiusX;
        }
        drawDepthLine(activeEditor, p0, p1, viewport.depthCameraState(), depthOperationColor(viewport.brushSettings().amount, true));
    }

    override
    void drawOptions(DepthEditViewport viewport) {
        igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
            if (incButtonColored(__("Create"), ImVec2(0, 0), toolMode == RingToolMode.Create ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                setMode(RingToolMode.Create);
            }
            incTooltip(_("Create depth rings"));
            igSameLine(0, 0);
            if (incButtonColored(__("Edit"), ImVec2(0, 0), toolMode == RingToolMode.Edit ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                setMode(RingToolMode.Edit);
            }
            incTooltip(_("Edit existing depth rings"));
        igPopStyleVar();
        igSameLine(0, 6);

        auto settings = &viewport.brushSettings();
        drawOptionDrag("Amount", &settings.amount, 0.01f, -2.0f, 2.0f, "%.2f");
        drawOptionDrag("Length", &settings.radiusX, 1.0f, 1.0f, 1000.0f, "%.0f");
        drawOptionDrag("Width", &settings.radiusY, 1.0f, 1.0f, 400.0f, "%.0f");
        drawOptionDrag("Angle", &settings.angle, 1.0f, -180.0f, 180.0f, "%.0f");
        drawOptionDrag("Falloff", &settings.hardness, 0.05f, 0.1f, 8.0f, "%.2f", false);
    }

    override
    bool update(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        if (incInputIsKeyPressed(ImGuiKey.Tab)) {
            toggleMode();
            return true;
        }

        if (toolMode == RingToolMode.Edit) {
            auto editorSet = viewport.getEditor();
            if (editorSet is null) return false;
            if (io.MouseDown[0] && editorSet.updateOperationDrag(
                screenMouseToView(io, camera),
                io.MousePos.y,
                viewport.depthCameraState(),
                viewport.brushSettings().snapToGrid
            )) {
                return true;
            }
            if (!io.MouseDown[0] && editorSet.endOperationDrag()) {
                return true;
            }
            if (!incInputIsMouseClicked(ImGuiMouseButton.Left)) return false;
            return editorSet.beginOperationDrag(
                screenMouseToView(io, camera),
                io.MousePos.y,
                viewport.depthCameraState(),
                &isRingOperation
            );
        }

        if (activeEditor !is null) {
            if (io.MouseDown[0]) {
                DepthMeshEditorOne editor;
                ptrdiff_t index;
                if (nearest(io, camera, viewport, editor, index) && editor is activeEditor) {
                    currentVertex = index;
                    currentPoint = viewport.brushSettings().snapToGrid
                        ? activeEditor.localVertex(cast(size_t)index)
                        : localMouse(io, camera, viewport);
                }
                return true;
            }
            auto editorSet = viewport.getEditor();
            if (auto op = buildOperation(viewport)) {
                auto ctx = new Context();
                cmd!(DepthEditorOperationCommand.AddEditorDepthOp)(ctx, editorSet, activeEditor, op, -1);
            }
            resetCreateDrag();
            return true;
        }

        if (!incInputIsMouseClicked(ImGuiMouseButton.Left)) return false;
        ptrdiff_t index;
        if (!nearest(io, camera, viewport, activeEditor, index)) return false;
        startVertex = index;
        currentVertex = index;
        startPoint = viewport.brushSettings().snapToGrid
            ? activeEditor.localVertex(cast(size_t)index)
            : localMouse(io, camera, viewport);
        currentPoint = startPoint;
        return true;
    }
}
