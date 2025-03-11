module nijigenerate.viewport.vertex.mesheditor.drawable;

import i18n;
import nijigenerate.panels.inspector.part;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.common.mesheditor.brushes;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.common.spline;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.core.math.mesh;
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
import std.algorithm;
import std.stdio;
import std.range: enumerate;
import std.array;
import std.typecons;

/**
 * MeshEditor of Drawable for vertex operation.
 */
class IncMeshEditorOneFor(T: Drawable, EditMode mode: EditMode.VertexEdit) : IncMeshEditorOneDrawable {
protected:
    override
    void substituteMeshVertices(MeshVertex* meshVertex) {
        mesh.vertices ~= meshVertex;
    }
    IncMesh previewMesh;
    MeshEditorAction!DeformationAction editorAction = null;

public:
    this() {
        super(false);
    }

    override
    void setTarget(Node target) {
        Drawable drawable = cast(Drawable)target;
        if (drawable is null)
            return;
        super.setTarget(target);
        transform = target ? target.transform.matrix : mat4.identity;
        mesh = new IncMesh(drawable.getMesh());
        refreshMesh();
    }

    override
    void resetMesh() {
        mesh.reset();
    }

    override
    void refreshMesh() {
        mesh.refresh();
        if (previewingTriangulation()) {
            previewMesh = mesh.autoTriangulate();
        } else {
            previewMesh = null;
        }
        updateMirrorSelected();
    }

    override
    void importMesh(ref MeshData data) {
        mesh.import_(data);
        mesh.refresh();
    }

    override
    void mergeMesh(ref MeshData data, mat4 matrix) {
        mesh.merge_(data, matrix);
        mesh.refresh();
    }

    override
    void applyOffsets(vec2[] offsets) {
    }

    override
    vec2[] getOffsets() {
        return null;
    }

    override
    void applyToTarget() {
        applyMeshToTarget(target, mesh.vertices, &mesh);
        foreach (welded; target.welded) {
            incRegisterWeldedPoints(target, welded.target);
        }
    }

    override
    void applyPreview() {
        mesh = previewMesh;
        previewMesh = null;
        previewTriangulate = false;
    }                      

    override
    void pushDeformAction() {
        if (editorAction && editorAction.action.dirty) {
            editorAction.updateNewState();
            incActionPush(editorAction);
            editorAction = null;
        }        
    }

    override
    ulong getVertexFromPoint(vec2 mousePos) {
        return mesh.getVertexFromPoint(mousePos, incViewportZoom);
    }

    override
    float[] getVerticesInBrush(vec2 mousePos, Brush brush) {
        return mesh.getVerticesInBrush(mousePos, brush);
    }

    override
    void removeVertexAt(vec2 vertex) {
        mesh.removeVertexAt(vertex, incViewportZoom);
    }

    override
    bool removeVertex(ImGuiIO* io, bool selectedOnly) {
        // In the case that it is, double clicking would remove an item
        if (!selectedOnly || isSelected(vtxAtMouse)) {
            foreachMirror((uint axis) {
                removeVertexAt(mirror(axis, mousePos));
            });
            refreshMesh();
            vertexMapDirty = true;
            selected.length = 0;
            updateMirrorSelected();
            maybeSelectOne = ulong(-1);
            vtxAtMouse = ulong(-1);
            return true;
        }
        return false;
    }

    override
    bool addVertex(ImGuiIO* io) {
        ulong off = mesh.vertices.length;
        if (isOnMirror(mousePos, meshEditAOE)) {
            placeOnMirror(mousePos, meshEditAOE);
        } else {
            foreachMirror((uint axis) {
                substituteMeshVertices(new MeshVertex(mirror(axis, mousePos)));
            });
        }
        refreshMesh();
        vertexMapDirty = true;
        if (io.KeyCtrl) selectOne(mesh.vertices.length - 1);
        else selectOne(off);
        return true;
    }

    override
    bool updateChanged(bool changed) {
        if (changed)
            mesh.changed = true;

        if (mesh.changed) {
            if (previewingTriangulation())
                previewMesh = mesh.autoTriangulate();
            mesh.changed = false;
        }
        return changed;
    }

    override
    void addMeshVertex(MeshVertex* v2) {
        mesh.vertices ~= v2;
    }

    override
    int indexOfMesh(MeshVertex* v2) {
        return cast(int)mesh.vertices.countUntil(v2);
    }

    override
    void insertMeshVertex(int index, MeshVertex* v2) {
        mesh.vertices.insertInPlace(index, v2);
    }

    override
    void removeMeshVertex(MeshVertex* v2) {
        mesh.remove(v2);
    }

    override
    void moveMeshVertex(MeshVertex* v, vec2 newPos) {
        v.position = newPos;
    }

    override
    bool isPointOver(vec2 mousePos) {
        return mesh.isPointOverVertex(mousePos, incViewportZoom);
    }

    override
    ulong[] getInRect(vec2 min, vec2 max, uint groupId = 0) { 
        return mesh.getInRect(selectOrigin, mousePos, groupId);
    }
    override 
    MeshVertex*[] getVerticesByIndex(ulong[] indices, bool removeNull = false) {
        MeshVertex*[] result;
        foreach (idx; indices) {
            if (idx < mesh.vertices.length)
                result ~= mesh.vertices[idx];
            else if (!removeNull)
                result ~= null;
        }
        return result;
    }

    override
    void createPathTarget() {
        getPath().createTarget(mesh, mat4.identity); //transform.inverse() * target.transform.matrix);
    }

    override
    mat4 updatePathTarget() {
        return getPath().updateTarget(mesh, selected);
    }

    override
    void resetPathTarget() {
        getPath().resetTarget(mesh);
    }

    override
    void remapPathTarget(ref CatmullSpline p, mat4 trans) {
        p.remapTarget(mesh, mat4.identity);
    }

    override
    bool hasAction() { return editorAction !is null; }

    override
    void updateAddVertexAction(MeshVertex* vertex) {
        if (editorAction) {
            editorAction.action.addVertex(vertex);
        }
    }

    override
    void clearAction() {
        if (editorAction)
            editorAction.action.clear();
    }

    override
    void markActionDirty() {
        if (editorAction)
            editorAction.action.markAsDirty();
    }

    Action getDeformActionImpl(bool reset = false)() {
        if (reset)
            pushDeformAction();
        if (editorAction is null || !editorAction.action.isApplyable()) {
            auto deformAction = new DeformationAction(target.name, target);
            editorAction = tools[toolMode].editorAction(target, deformAction);

        } else {
            if (reset)
                editorAction.clear();
        }
        return editorAction;
    }

    override
    Action getDeformAction() {
        return getDeformActionImpl!false();
    }

    override
    Action getCleanDeformAction() {
        return getDeformActionImpl!true();
    }

    override
    void forceResetAction() {
        editorAction = null;
    }

    override
    void draw(Camera camera) {
        mat4 trans = mat4.identity;

        if (vtxAtMouse != ulong(-1) && !isSelecting) {
            MeshVertex*[] one = getVerticesByIndex([vtxAtMouse], true);
            mesh.drawPointSubset(one, vec4(1, 1, 1, 0.3), trans, 15);
        }

        if (previewMesh) {
            previewMesh.drawLines(trans, vec4(0.7, 0.7, 0, 1));
            mesh.drawPoints(trans);
        } else {
            Tuple!(ptrdiff_t[], vec4)[] indices;
            if (auto drawable = cast(Drawable)getTarget()) {
                if (drawable.welded) {
                    foreach (welded; drawable.welded) {
                        auto t = tuple(welded.indices.enumerate.filter!(i=>i[1]!=cast(ptrdiff_t)-1).map!((p)=>cast(ptrdiff_t)p[0]).array, vec4(0, 0.7, 0.7, 1));
                        if (t[0].length > 0)
                            indices ~= t;
                    }
                }
            }
            mesh.draw(trans, vertexColor, edgeColor, indices.length > 0? indices: null);
        }

        if (groupId != 0) {
            MeshVertex*[] vertsInGroup = [];
            foreach (v; mesh.vertices) {
                if (v.groupId != groupId) vertsInGroup ~= v;
            }
            mesh.drawPointSubset(vertsInGroup, vec4(0.6, 0.6, 0.6, 1), trans);
        }

        if (selected.length) {
            if (isSelecting && !mutateSelection) {
                auto selectedVertices = getVerticesByIndex(selected, true);
                mesh.drawPointSubset(selectedVertices, vec4(0.6, 0, 0, 1), trans);
            }
            else {
                auto selectedVertices = getVerticesByIndex(selected, true);
                mesh.drawPointSubset(selectedVertices, vec4(1, 0, 0, 1), trans);
            }
        }

        if (mirrorSelected.length) {
            auto mirroredVertices = getVerticesByIndex(mirrorSelected, true);
            mesh.drawPointSubset(mirroredVertices, vec4(1, 0, 1, 1), trans);
        }

        if (isSelecting) {
            vec3[] rectLines = incCreateRectBuffer(selectOrigin, mousePos);
            inDbgSetBuffer(rectLines);
            if (!mutateSelection) inDbgDrawLines(vec4(1, 0, 0, 1), trans);
            else if(invertSelection) inDbgDrawLines(vec4(0, 1, 1, 0.8), trans);
            else inDbgDrawLines(vec4(0, 1, 0, 0.8), trans);

            if (newSelected.length) {
                if (mutateSelection && invertSelection) {
                    auto newSelectedVertices = getVerticesByIndex(newSelected, true);
                    mesh.drawPointSubset(newSelectedVertices, vec4(1, 0, 1, 1), trans);
                }
                else {
                    auto newSelectedVertices = getVerticesByIndex(newSelected, true);
                    mesh.drawPointSubset(newSelectedVertices, vec4(1, 0, 0, 1), trans);
                }
            }
        }

        vec2 camSize = camera.getRealSize();
        vec2 camPosition = camera.position;
        vec3[] axisLines;
        if (mirrorHoriz) {
            axisLines ~= incCreateLineBuffer(
                vec2(mirrorOrigin.x, -camSize.y - camPosition.y),
                vec2(mirrorOrigin.x, camSize.y - camPosition.y)
            );
        }
        if (mirrorVert) {
            axisLines ~= incCreateLineBuffer(
                vec2(-camSize.x - camPosition.x, mirrorOrigin.y),
                vec2(camSize.x - camPosition.x, mirrorOrigin.y)
            );
        }

        if (axisLines.length > 0) {
            inDbgSetBuffer(axisLines);
            inDbgDrawLines(vec4(0.8, 0, 0.8, 1), trans);
        }

        if (toolMode in tools)
            tools[toolMode].draw(camera, this);
    }

    override
    void adjustPathTransform() {
        mat4 trans = (target? target.transform.matrix: transform).inverse * transform;
        ref CatmullSpline doAdjust(ref CatmullSpline p) {
            for (int i; i < p.points.length; i++) {
                p.points[i].position = (trans * vec4(p.points[i].position, 0, 1)).xy;
            }
            p.update();
            remapPathTarget(p, mat4.identity);
            return p;
        }
        if (getPath()) {
            if (getPath().target)
                getPath().target = doAdjust(getPath().target);
            auto path = getPath();
            setPath(doAdjust(path));
        }
        lastMousePos = (trans * vec4(lastMousePos, 0, 1)).xy;
        transform = this.target.transform.matrix;
        forceResetAction();
    }

}