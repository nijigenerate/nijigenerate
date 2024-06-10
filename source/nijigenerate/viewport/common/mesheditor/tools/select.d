module nijigenerate.viewport.common.mesheditor.tools.select;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.operations;
import i18n;
import nijigenerate.viewport;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate;
import nijilife;
import nijilife.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
import std.stdio;

class NodeSelect : Tool, Draggable {
    bool isDragging = false;

    enum SelectActionID {
        None = 0,
        SelectArea = 1,
        ToggleSelect,
        SelectOne,
        MaybeSelectOne,
        SelectMaybeSelectOne,
        StartDrag,
        End
    }

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        assert(!impl.deformOnly || toolMode != VertexToolMode.Connect);
        isDragging = false;
        impl.isSelecting = false;
        incViewportSetAlwaysUpdate(false);
    }

    override 
    int peek(ImGuiIO* io, IncMeshEditorOne impl) {
        impl.lastMousePos = impl.mousePos;

        impl.mousePos = incInputGetMousePosition();
        if (impl.deformOnly) {
            vec4 pIn = vec4(-impl.mousePos.x, -impl.mousePos.y, 0, 1);
            mat4 tr = impl.transform.inverse();
            vec4 pOut = tr * pIn;
           impl. mousePos = vec2(pOut.x, pOut.y);
        } else {
            impl.mousePos = -impl.mousePos;
        }

        impl.vtxAtMouse = impl.getVertexFromPoint(impl.mousePos);

        return 0;
    }

    override 
    int unify(int[] actions) {
        return 0;
    }

    override 
    bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        return false;
    }

    override 
    bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
        if (!impl.isSelecting) {
            isDragging = true;
            impl.getDeformAction();
            return true;
        }
        return false;
    }

    override 
    bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl) {
        isDragging = false;
        if (impl.isSelecting) {
            if (impl.mutateSelection) {
                if (!impl.invertSelection) {
                    foreach(v; impl.newSelected) {
                        auto idx = impl.selected.countUntil(v);
                        if (idx == -1) impl.selected ~= v;
                    }
                } else {
                    foreach(v; impl.newSelected) {
                        auto idx = impl.selected.countUntil(v);
                        if (idx != -1) impl.selected = impl.selected.remove(idx);
                    }
                }
                impl.updateMirrorSelected();
                impl.newSelected.length = 0;
            } else {
                impl.selected = impl.newSelected;
                impl.newSelected = [];
                impl.updateMirrorSelected();
            }
            impl.isSelecting = false;
        }
        impl.pushDeformAction();
        return true;
    }

    override 
    bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl) {
        if (isDragging) {
            foreach(select; impl.selected) {
                impl.foreachMirror((uint axis) {
                    MeshVertex *v = impl.getVerticesByIndex([impl.mirrorVertex(axis, select)])[0];
                    if (v is null) return;
                    impl.updateAddVertexAction(v);
                    impl.markActionDirty();
                    v.position += impl.mirror(axis, mousePos - impl.lastMousePos);
                });
            }
            if (impl.selected.length > 0)
                impl.maybeSelectOne = ulong(-1);
            impl.refreshMesh();
            return true;
        }

        return false;
    }

    override
    void draw(Camera camera, IncMeshEditorOne impl) {
    }
}