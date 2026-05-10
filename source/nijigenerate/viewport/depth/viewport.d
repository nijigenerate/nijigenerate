/*
    Depth editing viewport.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.viewport;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.core.input;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.mesheditor;
import nijigenerate.viewport.depth.tools;
import nijigenerate.widgets.button;
import nijigenerate.widgets.tooltip;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.algorithm : clamp, max;

class DepthEditViewport : Viewport {
private:
    Node[] selection;
    DepthToolMode toolMode = DepthToolMode.Select;
    DepthMeshEditor editor;
    DepthCamera3D depthCamera;
    DepthBrushSettings brush;

    void syncTargets() {
        if (editor !is null) editor.setTargets(selection);
    }

    void leaveToModel(GridDeformer[] targets) {
        incSetEditMode(EditMode.ModelEdit);
        foreach (target; targets) incAddSelectNode(target);
        if (targets.length > 0) focusGridOrigin(targets[0]);
    }

    bool updateDepthCamera(ImGuiIO* io) {
        bool changed = false;
        if (io.MouseDown[1] && !io.KeyShift && incInputIsDragRequested(ImGuiMouseButton.Right)) {
            depthCamera.yaw += io.MouseDelta.x * 0.01f;
            depthCamera.pitch = clamp(depthCamera.pitch + io.MouseDelta.y * 0.01f, -1.35f, 1.35f);
            changed = true;
        }
        if (io.MouseWheel != 0) {
            depthCamera.zoom = clamp(depthCamera.zoom * (1 + io.MouseWheel * 0.08f), 0.1f, 8.0f);
            changed = true;
        }
        return changed;
    }

public:
    override
    void present() {
        editor = new DepthMeshEditor();
        syncTargets();
        auto targets = editor.getTargets();
        if (targets.length > 0) focusGridOrigin(targets[0]);
    }

    override
    void withdraw() {
        if (editor !is null) {
            editor.closeStack();
            editor.dispose();
            editor = null;
        }
    }

    override
    void selectionChanged(Node[] selection) {
        this.selection = selection;
        syncTargets();
    }

    DepthToolMode activeToolMode() {
        return toolMode;
    }

    DepthEditTool activeTool() {
        return incDepthEditTool(toolMode);
    }

    void setToolMode(DepthToolMode mode) {
        toolMode = mode;
        if (auto tool = activeTool()) {
            tool.selected(this);
        }
    }

    DepthMeshEditor getEditor() {
        return editor;
    }

    ref DepthBrushSettings brushSettings() {
        return brush;
    }

    ref DepthCamera3D depthCameraState() {
        return depthCamera;
    }

    void drawDepthOptions() {
        igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
        if (incButtonColored(__(" Snap"), ImVec2(0, 0), brush.snapToGrid ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
            brush.snapToGrid = !brush.snapToGrid;
        }
        incTooltip(brush.snapToGrid ? _("Snap tool points to grid vertices") : _("Use free tool points"));
        igPopStyleVar();
    }

    override
    void draw(Camera camera) {
        incActivePuppet.update();
        if (editor !is null) {
            editor.draw(camera, depthCamera);
        }

        if (auto tool = activeTool()) {
            tool.draw(camera, this);
        }
    }

    override
    void drawTools() {
        igSetWindowFontScale(1.30);
            igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(1, 1));
            igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(8, 10));
                foreach (tool; incDepthEditTools()) {
                    if (tool.drawToolButton(toolMode)) {
                        setToolMode(tool.mode);
                    }
                }
            igPopStyleVar(2);
        igSetWindowFontScale(1);
    }

    override
    void drawOptions() {
        drawDepthOptions();
        if (auto tool = activeTool()) {
            igSeparator();
            tool.drawOptions(this);
        }
        if (editor !is null) {
            igSeparator();
            editor.drawOperationOptions();
        }
    }

    override
    void drawConfirmBar() {
        if (editor is null) return;
        auto targets = editor.getTargets();
        igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(16, 4));
            if (incButtonColored(__(" Apply"), ImVec2(0, 26))) {
                editor.closeStack();
                editor.applyToTargets();
                leaveToModel(targets);
            }
            incTooltip(_("Apply"));

            igSameLine(0, 0);

            if (incButtonColored(__(" Cancel"), ImVec2(0, 26))) {
                if (igGetIO().KeyShift) {
                    editor.resetFromTargets();
                }
                editor.closeStack();
                leaveToModel(targets);
            }
            incTooltip(_("Cancel"));
        igPopStyleVar();
    }

    override
    void update(ImGuiIO* io, Camera camera) {
        bool consumed = false;
        if (auto tool = activeTool()) {
            consumed = tool.update(io, camera, this);
        }
        if (!consumed) updateDepthCamera(io);
        if (editor !is null) editor.update(io, camera);
    }
}
