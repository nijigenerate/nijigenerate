/*
    Copyright © 2020-2024, Inochi2D Project
    Copyright ©      2025, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Authors: Lin, Yong Xiang <r888800009@gmail.com>
*/
module nijigenerate.viewport.common.mesheditor.tools.lasso;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.model.mesheditor;
import nijigenerate.widgets;
import nijigenerate : EditMode, incEditMode;
import i18n;
import bindbc.imgui;
import nijilive;
import nijilive.core.dbg;
import std.algorithm.mutation : swap;
import std.algorithm;
import std.array;
import nijigenerate.core.math.triangle;

/**
    the PolyLassoTool allow to draw a polygon and undo the last point
    the RegualarLassoTool just click and drag to draw a lasso selection
*/
enum LassoType {
    PolyLasso = 0,
    RegularLasso = 1,
}

private {
    const char* [LassoType] lassoTypeIcons = [
        LassoType.PolyLasso: "\ue922", // google material icon "timeline"
        LassoType.RegularLasso: "\ue155", // google material icon "gesture"
    ];

    string getLassoHint(LassoType lassoType) {
        switch (lassoType) {
            case LassoType.PolyLasso: return _("Poly Lasso Selection Mode");
            case LassoType.RegularLasso: return _("Regular Lasso Selection Mode");
            default:
                throw new Exception("Invalid LassoType");
        }
    }

    LassoType getNextLassoType(LassoType lassoType) {
        switch (lassoType) {
            case LassoType.PolyLasso: return LassoType.RegularLasso;
            case LassoType.RegularLasso: return LassoType.PolyLasso;
            default:
                throw new Exception("Invalid LassoType");
        }
    }
}

class LassoIO {
    bool addSelect = false;
    bool removeSelect = false;
    bool undo = false;
    bool cleanup = false;

    void update() {
        addSelect = igIsKeyDown(ImGuiKey.ModShift);
        removeSelect = igIsKeyDown(ImGuiKey.ModCtrl);
        undo = igIsMouseClicked(ImGuiMouseButton.Right);
        cleanup = igIsKeyPressed(ImGuiKey.Escape);
    }
}

class LassoTool : NodeSelect {
private:
    vec2[] lassoPoints;
    size_t[] rollbackCheckpoints;

public:
    LassoType lassoType = LassoType.RegularLasso;

    void setNextMode() {
        lassoType = getNextLassoType(lassoType);
        cleanup();
    }

    void cleanup() {
        lassoPoints.length = 0;
        rollbackCheckpoints.length = 0;
    }

    /**
        Rollback the previous checkpoint
    */
    bool rollbackOnce() {
        if (rollbackCheckpoints.length == 0)
            return false;
        
        lassoPoints.length = rollbackCheckpoints[$ - 1];
        rollbackCheckpoints.length -= 1;
        return true;
    }

    void commitCheckpoint() {
        rollbackCheckpoints ~= lassoPoints.length;
    }

    void doSelection(IncMeshEditorOne impl, LassoIO lassoIO) {
        bool addSelect = lassoIO.addSelect;
        bool removeSelect = lassoIO.removeSelect;

        // lassoPoints is stored the edge so multiply by 2
        if (lassoPoints.length < 2 * 2)
            return;

        auto indices = impl.filterVertices((MeshVertex* v) => pointInPolygon(v, lassoPoints, impl.getGroupId));

        // check if the point is inside the lasso polygon
        if (!addSelect && !removeSelect)
            impl.deselectAll();

        foreach (index; indices) {
            if (addSelect && removeSelect) impl.toggleSelect(index); 
            else if(!addSelect && removeSelect) impl.deselect(index);
            else impl.select(index);
        }

        cleanup();
    }

    vec3[] mirrorLassoPoints(IncMeshEditorOne impl, uint axis, vec3[] points) {
        vec3[] mirroredPoints;
        foreach (point; points) {
            vec2 v2 = impl.mirrorDelta(axis, point.xy);
            mirroredPoints ~= vec3(v2.x, v2.y, 0);
        }
        return mirroredPoints;
    }



    /**
        if trigger the doSelection, return true
    */
    bool doSelectionTrigger(IncMeshEditorOne impl, LassoIO lassoIO) {
        if (lassoPoints.length > 2) {
            // force close the polygon prevent issue
            lassoPoints[$ - 1] = lassoPoints[0];
            doSelection(impl, lassoIO);
            return true;
        }

        return false;
    }

    override
    bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        super.update(io, impl, action, changed);
        LassoIO lassoIO = new LassoIO;
        lassoIO.update();

        incStatusTooltip(_("Add Lasso Point"), _("Left Mouse"));
        incStatusTooltip(_("Additive Selection"), _("Shift"));
        if (lassoIO.addSelect) incStatusTooltip(_("Inverse Selection"), _("Ctrl"));
        else incStatusTooltip(_("Remove Selection"), _("Ctrl"));
        incStatusTooltip(_("Delete Last Lasso Point"), _("Right Mouse"));
        incStatusTooltip(_("Clear All Lasso Points"), _("ESC"));

        if (igIsMouseClicked(ImGuiMouseButton.Left))
            commitCheckpoint();

        if (igIsMouseClicked(ImGuiMouseButton.Left) ||
            (igIsMouseDown(ImGuiMouseButton.Left) && lassoPoints.length > 0 &&
            lassoPoints[$ - 1].xy.distance(impl.mousePos) > 14/incViewportZoom)) {

            if (lassoPoints.length > 1)
                lassoPoints ~= lassoPoints[$ - 1];
            lassoPoints ~= impl.mousePos;

            if (isClosestToStart(impl.mousePos) == 0)
                doSelectionTrigger(impl, lassoIO);
        }

        if (igIsMouseReleased(ImGuiMouseButton.Left) && lassoType == LassoType.RegularLasso)
            doSelectionTrigger(impl, lassoIO);

        if (lassoIO.undo)
            rollbackOnce();

        if (lassoIO.cleanup)
            cleanup();

        return true;
    }

    bool isCloses(vec2 p1, vec2 p2) {
        return p1.distance(p2) < 14/incViewportZoom;
    }

    size_t findClosest(vec2 target) {
        foreach (i, p; lassoPoints) {
            if (!isCloses(p, target))
                continue;
            return i;
        }
        return -1;
    }

    size_t isClosestToStart(vec2 target) {
        if (lassoPoints.length == 0)
            return -1;
        return isCloses(lassoPoints[0], target) ? 0 : -1;
    }

    override
    void draw(Camera camera, IncMeshEditorOne impl) {
        super.draw(camera, impl);

        if (lassoPoints.length == 0) {
            return;
        }

        mat4 transform = mat4.identity;
        if (impl.deformOnly)
            transform = impl.transform;

//        impl.foreachMirror((uint axis) {
//            vec2[] mirroredPoints = mirrorLassoPoints(impl, axis, lassoPoints);
            auto mirroredPoints = lassoPoints.dup;

            // find closest point
            vec2 mousePos = impl.mousePos;
            size_t p = isClosestToStart(mousePos);
            if (p != -1) {
                inDbgSetBuffer([vec3(mirroredPoints[p], 0)]);
                inDbgPointsSize(10);
                inDbgDrawPoints(vec4(1, 0, 0, 1), transform);
            } else if (lassoType == LassoType.PolyLasso) {
                // draw the first point to hint the user to close the polygon
                inDbgSetBuffer([vec3(mirroredPoints[0], 0)]);
                inDbgPointsSize(7);
                inDbgDrawPoints(vec4(0.6, 0.6, 0.6, 0.6), transform);
            }

            inDbgSetBuffer((mirroredPoints ~ [mirroredPoints[$ - 1], impl.mousePos]).map!((i)=>vec3(i, 0)).array);
            inDbgLineWidth(3);
            inDbgDrawLines(vec4(.0, .0, .0, 1), transform);
            inDbgLineWidth(1);
            inDbgDrawLines(vec4(1, 1, 1, 1), transform);
//        });
    }
}

class ToolInfoImpl(T: LassoTool) : ToolInfoBase!(T) {
    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        return super.viewportTools(deformOnly, toolMode, editors);
    }
    override VertexToolMode mode() { return VertexToolMode.LassoSelection; }

    // material symbols "\ueb03" lasso_select not working
    // using material icons (deprecated) highlight_alt instead
    override string icon() { return "\uef52"; }
    override string description() { return _("Lasso Selection"); }

    override
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) { 
        auto lassoTool = cast(LassoTool)(editors.length == 0 ? null: editors.values()[0].getTool());
        igBeginGroup();
            auto current_icon = lassoTypeIcons[lassoTool.lassoType];
            if (incButtonColored(current_icon, ImVec2(0, 0), ImVec4.init)) {
                foreach (e; editors) {
                    auto lt = cast(LassoTool)(e.getTool());
                    if (lt !is null)
                        lt.setNextMode();
                }
            }
            incTooltip(getLassoHint(lassoTool.lassoType));
        igEndGroup();


        return false;
    }
}