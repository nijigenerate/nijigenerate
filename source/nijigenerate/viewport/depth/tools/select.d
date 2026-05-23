/*
    Depth edit selection tool.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.select;

import bindbc.imgui;
import i18n;
import nijigenerate.core.input;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijigenerate.viewport.depth.tools.base;
import nijilive;
import std.algorithm : max;

class DepthSelectTool : DepthEditTool {
    override DepthToolMode mode() { return DepthToolMode.Select; }
    override const(char)* icon() { return __(""); }
    override string tooltip() { return _("Select Depth Vertices"); }

    override
    bool update(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        auto editorSet = viewport.getEditor();
        if (editorSet is null) return false;
        if (io.MouseDown[0] && editorSet.updateOperationDrag(screenMouseToView(io, camera), io.MousePos.y, viewport.depthCameraState(), viewport.brushSettings().snapToGrid)) {
            return true;
        }
        if (!io.MouseDown[0] && editorSet.endOperationDrag()) {
            return true;
        }
        if (!incInputIsMouseClicked(ImGuiMouseButton.Left)) return false;
        if (editorSet.beginOperationDrag(screenMouseToView(io, camera), io.MousePos.y, viewport.depthCameraState())) {
            return true;
        }

        ptrdiff_t index;
        auto editor = nearestVertexFromScreenMouse(io, camera, viewport, 14.0f / max(0.01f, incViewportZoom), index);
        if (editor is null || index < 0) return false;
        editor.selectVertex(index);
        return true;
    }
}
