module nijigenerate.viewport.vertex.mesheditor.tools.edgecutter;

/*
    EdgeCutter Tool
    - Draw a line and disconnect every mesh edge that intersects the line.
    - No mirroring or quadrant replication; uses the exact user-drawn segment only.
    - Operates via IncMeshEditorOne interfaces (no direct mesh access, no casting contracts).
*/

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.base;
import i18n;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijilive;
import nijilive.core.dbg;
import nijilive.math;
import bindbc.imgui;
import nijigenerate.widgets;
import std.algorithm.searching : all;

class EdgeCutterTool : NodeSelect {
    vec2 dragOrigin;
    vec2 dragEnd;

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        isDragging = false;
    }

    override bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
        dragOrigin = mousePos;
        dragEnd = mousePos;
        isDragging = true;
        return true;
    }

    override bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl) {
        if (!isDragging) return false;
        dragEnd = mousePos;
        return true;
    }

    override bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl) {
        if (!isDragging) return false;
        isDragging = false;
        dragEnd = mousePos;

        // Disconnect edges that intersect with the cutter line using impl interface (no casting)
        auto action = impl.newMeshDisconnectAction(impl.getTarget().name);

        vec2 a = dragOrigin;
        vec2 b = dragEnd;

        impl.forEachEdge((MeshVertex* v, MeshVertex* conn) {
            // Disconnect only when the user-drawn segment [a,b] intersects edge [v,conn]
            if (areLineSegmentsIntersecting(a, b, v.position, conn.position)) {
                action.disconnect(v, conn);
            }
        });

        action.updateNewState();
        incActionPush(action);

        impl.refreshMesh();
        return true;
    }

    override bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        super.update(io, impl, action, changed);

        incStatusTooltip(_("Cut edges intersecting the line"), _("Drag Left Mouse"));

        // Start on mouse down (no drag threshold), end on release
        if (igIsMouseClicked(ImGuiMouseButton.Left)) {
            onDragStart(impl.mousePos, impl);
        }

        if (isDragging && igIsMouseDown(ImGuiMouseButton.Left)) {
            onDragUpdate(impl.mousePos, impl);
        }

        if (isDragging && incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
        }

        return true;
    }

    override void draw(Camera camera, IncMeshEditorOne impl) {
        super.draw(camera, impl);
        if (!isDragging) return;
        vec3[] lines = [vec3(dragOrigin, 0), vec3(dragEnd, 0)];
        inDbgSetBuffer(lines);
        inDbgLineWidth(3);
        inDbgDrawLines(vec4(0, 0, 0, 1), mat4.identity());
        inDbgDrawLines(vec4(1, 0, 0, 1), mat4.identity());
    }
}

class ToolInfoImpl(T: EdgeCutterTool) : ToolInfoBase!(T) {
    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (deformOnly)
            return false;

        bool isDrawable = editors.keys.all!(k => cast(Drawable)k !is null);
        if (isDrawable) {
            return super.viewportTools(deformOnly, toolMode, editors);
        }
        return false;
    }
    override bool canUse(bool deformOnly, Node[] targets) {
        if (deformOnly)
            return false;

        return targets.all!(k => cast(Drawable)k !is null);
    }
    override VertexToolMode mode() { return VertexToolMode.EdgeCutter; }
    override string icon() { return "\uf1f8"; }
    override string description() { return _("Edge Cutter"); }

    override
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) { return false; }
}


