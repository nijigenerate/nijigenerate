/*
    Depth attached point editing tool.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.attachedpoint;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.commands : Context, cmd;
import nijigenerate.commands.depth.editor : DepthEditorOperationCommand;
import nijigenerate.core.input;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.mesheditor;
import nijigenerate.viewport.depth.tools.base;
import nijigenerate.viewport.depth.tools.operation;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijilive;
import std.algorithm : max;

class DepthAttachedPointTool : DepthEditTool {
private:
    DepthMeshEditorOne activeEditor;
    ptrdiff_t activeVertex = -1;
    float dragOriginY;
    DepthAttachedPointOperation operation;

    bool beginDrag(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return false;

        ptrdiff_t index;
        auto editor = nearestVertexFromScreenMouse(io, camera, viewport, index);
        if (editor is null || index < 0) return false;

        activeEditor = editor;
        activeVertex = index;
        dragOriginY = io.MousePos.y;
        operation = new DepthAttachedPointOperation(cast(size_t)index, 0);
        editorSet.appendOperation(editor, operation);
        editor.selectVertex(index);
        return true;
    }

    void updateDrag(ImGuiIO* io, DepthEditViewport viewport) {
        if (activeEditor is null || activeVertex < 0 || operation is null) return;
        operation.amount = -(io.MousePos.y - dragOriginY) * 0.006f;
        viewport.getEditor().recompute(activeEditor);
    }

    void endDrag(DepthEditViewport viewport) {
        if (operation !is null && activeEditor !is null) {
            auto ctx = new Context();
            cmd!(DepthEditorOperationCommand.AddEditorDepthOp)(ctx, viewport.getEditor(), activeEditor, operation, -1);
        }
        activeEditor = null;
        activeVertex = -1;
        operation = null;
    }

public:
    override DepthToolMode mode() { return DepthToolMode.AttachedPoint; }
    override const(char)* icon() { return "\uEB35"; } // swipe_up_alt
    override string tooltip() { return _("Edit Attached Depth Point"); }

    override
    bool update(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        if (activeEditor !is null) {
            if (io.MouseDown[0]) {
                updateDrag(io, viewport);
                return true;
            }
            endDrag(viewport);
            return true;
        }

        if (incInputIsMouseClicked(ImGuiMouseButton.Left)) {
            return beginDrag(io, camera, viewport);
        }
        return false;
    }
}
