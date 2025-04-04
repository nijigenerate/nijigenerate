/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Authors:
    - Luna Nielsen
    - Asahi Lina
*/
module nijigenerate.viewport.common.mesheditor.operations.base;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.brushes;
import i18n;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
//import std.stdio;

class IncMeshEditorOne {
public:
    abstract void substituteMeshVertices(MeshVertex* meshVertex);
    abstract ulong getVertexFromPoint(vec2 mousePos);
    abstract float[] getVerticesInBrush(vec2 mousePos, Brush brush);
    abstract void removeVertexAt(vec2 vertex);
    abstract bool removeVertex(ImGuiIO* io, bool selectedOnly);
    abstract bool addVertex(ImGuiIO* io);
    abstract bool updateChanged(bool changed);
    abstract void addMeshVertex(MeshVertex* v2);
    abstract void insertMeshVertex(int index, MeshVertex* v2);
    abstract void removeMeshVertex(MeshVertex* v2);
    int  indexOfMesh(MeshVertex* v) { return -1; };
    abstract void moveMeshVertex(MeshVertex* v, vec2 newPos);
    
    abstract bool isPointOver(vec2 mousePos);
    abstract ulong[] getInRect(vec2 min, vec2 max, uint groupId);
    abstract MeshVertex*[] getVerticesByIndex(ulong[] indices, bool removeNull = false);
    abstract bool hasAction();
    abstract void updateAddVertexAction(MeshVertex* vertex);
    abstract void clearAction();
    abstract void markActionDirty();

    bool deformOnly = false;
    bool vertexMapDirty = false;

    VertexToolMode toolMode = VertexToolMode.Points;
    ulong[] selected;
    ulong[] mirrorSelected;
    ulong[] newSelected;

    vec2 lastMousePos;
    vec2 mousePos;

    bool isSelecting = false;
    bool mutateSelection = false;
    bool invertSelection = false;
    ulong maybeSelectOne;
    ulong vtxAtMouse;

    vec2 selectOrigin;
    IncMesh previewMesh;

    bool deforming = false;
    float meshEditAOE = 4;

    bool previewTriangulate = false;
    bool mirrorHoriz = false;
    bool mirrorVert = false;
    vec2 mirrorOrigin = vec2(0, 0);
    mat4 transform = mat4.identity;

    vec4 vertexColor = vec4(1, 1, 1, 1);
    vec4 edgeColor   = vec4(0.5, 0.5, 0.5, 1);

    bool isSelected(ulong vertIndex) {
        import std.algorithm.searching : canFind;
        return selected.canFind(vertIndex);
    }

    void toggleSelect(ulong vertIndex) {
        import std.algorithm.searching : countUntil;
        import std.algorithm.mutation : remove;
        auto idx = selected.countUntil(vertIndex);
        if (isSelected(vertIndex)) {
            selected = selected.remove(idx);
        } else {
            selected ~= vertIndex;
        }
        updateMirrorSelected();
    }

    ulong selectOne(ulong vertIndex) {
        ulong lastSel = -1;
        if (selected.length > 0) {
            lastSel = selected[$-1];
        }
        selected = [vertIndex];
        auto vert = getVerticesByIndex([vertIndex])[0];
        updateAddVertexAction(vert);
        if (lastSel != ulong(-1))
            updateMirrorSelected();
        return lastSel;
    }

    void deselectAll() {
        selected.length = 0;
        clearAction();
        updateMirrorSelected();
    }

    vec2 mirrorH(vec2 point) {
        return 2 * vec2(mirrorOrigin.x, 0) + vec2(-point.x, point.y);
    }

    vec2 mirrorV(vec2 point) {
        return 2 * vec2(0, mirrorOrigin.x) + vec2(point.x, -point.y);
    }

    vec2 mirrorHV(vec2 point) {
        return 2 * mirrorOrigin - point;
    }

    vec2 mirror(uint axis, vec2 point) {
        switch (axis) {
            case 0: return point;
            case 1: return mirrorH(point);
            case 2: return mirrorV(point);
            case 3: return mirrorHV(point);
            default: assert(false, "bad axis");
        }
    }

    vec2 mirrorDelta(uint axis, vec2 point) {
        switch (axis) {
            case 0: return point;
            case 1: return vec2(-point.x, point.y);
            case 2: return vec2(point.x, -point.y);
            case 3: return vec2(-point.x, -point.y);
            default: assert(false, "bad axis");
        }
    }

    ulong mirrorVertex(uint axis, ulong vtxIndex) {
        if (axis == 0) return vtxIndex;
        auto vtx = getVerticesByIndex([vtxIndex])[0];
        if (vtx is null) {
            return -1;
        }
        ulong vInd = getVertexFromPoint(mirror(axis, vtx.position));
        if (vInd == vtxIndex) return -1;
        return vInd;
    }

    MeshVertex* mirrorVertex(uint axis, MeshVertex* vtx) {
        if (axis == 0) return vtx;
        ulong vInd = getVertexFromPoint(mirror(axis, vtx.position));
        MeshVertex* v = getVerticesByIndex([vInd])[0];
        if (v is null || v == vtx) return null;
        return getVerticesByIndex([vInd])[0];
    }

    bool isOnMirror(vec2 pos, float aoe) {
        return 
            (mirrorVert && pos.y > -aoe && pos.y < aoe) ||
            (mirrorHoriz && pos.x > -aoe && pos.x < aoe);
    }

    bool isOnMirrorCenter(vec2 pos, float aoe) {
        return 
            (mirrorVert && pos.y > -aoe && pos.y < aoe) &&
            (mirrorHoriz && pos.x > -aoe && pos.x < aoe);
    }

    void placeOnMirror(vec2 pos, float aoe) {
        if (isOnMirror(pos, aoe)) {
            if (mirrorHoriz && mirrorVert && isOnMirrorCenter(pos, aoe)) pos = vec2(0, 0);
            else if (mirrorVert) pos.y = 0;
            else if (mirrorHoriz) pos.x = 0;
            substituteMeshVertices(new MeshVertex(pos));
        }
    }

    void foreachMirror(void delegate(uint axis) func) {
        if (mirrorHoriz) func(1);
        if (mirrorVert) func(2);
        if (mirrorHoriz && mirrorVert) func(3);
        func(0);
    }

    void updateMirrorSelected() {
        mirrorSelected.length = 0;
        if (!mirrorHoriz && !mirrorVert) return;

        // Avoid duplicate selections...
        ulong[] tmpSelected;
        foreach(v; selected) {
            if (mirrorSelected.canFind(v)) continue;
            tmpSelected ~= v;

            foreachMirror((uint axis) {
                ulong v2 = mirrorVertex(axis, v);
                if (v2 == ulong(-1)) return;
                if (axis != 0) {
                    if (!tmpSelected.canFind(v2) && !mirrorSelected.canFind(v2))
                        mirrorSelected ~= v2;
                }
            });
        }
        auto mirrorSelectedVertices = getVerticesByIndex(mirrorSelected);
        foreach (v; mirrorSelectedVertices) {
            updateAddVertexAction(v);
        }
        selected = tmpSelected;
    }

    this(bool deformOnly) {
        this.deformOnly = deformOnly;
    }

    VertexToolMode getToolMode() {
        return toolMode;
    }

    abstract void setToolMode(VertexToolMode toolMode);

    bool previewingTriangulation() {
         return previewTriangulate && toolMode == VertexToolMode.Points;
    }

    abstract Node getTarget();
    abstract void setTarget(Node target);
    abstract void resetMesh();
    abstract void refreshMesh();
    abstract void importMesh(ref MeshData data);
    abstract void mergeMesh(ref MeshData data, mat4 matrix);
    abstract void applyOffsets(vec2[] offsets);
    abstract vec2[] getOffsets();

    abstract void applyToTarget();
    abstract void applyPreview();
    abstract void createPathTarget();
    abstract mat4 updatePathTarget();
    abstract void resetPathTarget();
    abstract void remapPathTarget(ref CatmullSpline p, mat4 trans);

    abstract void pushDeformAction();
    abstract Action getDeformAction();
    abstract Action getCleanDeformAction();
    abstract void forceResetAction();


    abstract int peek(ImGuiIO* io, Camera camera);
    abstract int unify(int[] actions);
    abstract bool update(ImGuiIO* io, Camera camera, int actions);
    abstract void draw(Camera camera);

    // getPath / setPath is remained for compatibility. should be migrated to implementation of PathDeformTool
    abstract CatmullSpline getPath();
    abstract void setPath(CatmullSpline path);

    abstract void adjustPathTransform();
    abstract Tool getTool();

    abstract uint getGroupId();
    abstract void setGroupId(uint groupId);
}
