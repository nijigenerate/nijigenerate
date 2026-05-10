/*
    Depth landmark selection tool.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.landmark;

import bindbc.imgui;
import i18n;
import nijigenerate.core.input;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijigenerate.viewport.depth.tools.base;
import nijilive;
import std.algorithm : max;

class DepthLandmarkTool : DepthEditTool {
    override DepthToolMode mode() { return DepthToolMode.Landmark; }
    override const(char)* icon() { return __("󰓹"); }
    override string tooltip() { return _("Mark Depth Reference Vertex"); }

    override
    bool update(ImGuiIO* io, Camera camera, DepthEditViewport viewport) {
        if (!incInputIsMouseClicked(ImGuiMouseButton.Left)) return false;
        ptrdiff_t index;
        auto editor = nearestVertexFromScreenMouse(io, camera, viewport, 14.0f / max(0.01f, incViewportZoom), index);
        if (editor is null || index < 0) return false;
        editor.selectVertex(index);
        return true;
    }
}
