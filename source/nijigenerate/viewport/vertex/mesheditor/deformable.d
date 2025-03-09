module nijigenerate.viewport.vertex.mesheditor.deformable;

import i18n;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.common.mesheditor.brushes;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.common.spline;
import nijigenerate.core.input;
import nijigenerate.core.math;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate;
import nijilive;
import nijilive.core.nodes.utils: removeByValue;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
import std.stdio;
import std.range: enumerate;
import std.algorithm: map;
import std.array;
/**
 * MeshEditor of Deformable for vertex operation.
 */
class IncMeshEditorOneFor(T: Deformable, EditMode mode: EditMode.VertexEdit) : IncMeshEditorOneDeformable if (!is(T: Drawable)) {
protected:
    override
    void substituteMeshVertices(MeshVertex* meshVertex) {
        deformable.vertices ~= meshVertex.position;
    }
    MeshEditorAction!DeformationAction editorAction = null;

public:
    this() {
        super(false);
    }

    override
    void setTarget(Node target) {
        Deformable deformable = cast(Deformable)target;
        if (deformable is null)
            return;
        super.setTarget(target);
        this.vertices = getTarget().getVertices().toMVertices;
        transform = target ? target.transform.matrix : mat4.identity;
    }

    override
    void resetMesh() {
        this.vertices = getTarget().getVertices().toMVertices;
        // TBD
    }

    override
    void refreshMesh() {
        // TBD
        updateMirrorSelected();
    }

    override
    void importMesh(ref MeshData data) {
        this.vertices.length = 0;
        this.vertices ~= data.vertices.map!((vec2 vtx) { return new MeshVertex(vtx); }).array;
    }

    override
    void mergeMesh(ref MeshData data, mat4 matrix) {
        this.vertices ~= data.vertices.map!((vec2 vtx) { return new MeshVertex(vtx); }).array;
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
        applyMeshToTarget(target, vertices, cast(IncMesh*)null);
    }

    override
    void applyPreview() {}                      

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
        return nijigenerate.core.math.vertex.getVertexFromPoint(vertices, mousePos, incViewportZoom);
    }

    override
    float[] getVerticesInBrush(vec2 mousePos, Brush brush) {
        return nijigenerate.core.math.vertex.getVerticesInBrush(vertices, mousePos, brush);
    }

    override
    void removeVertexAt(vec2 vertex) {
        nijigenerate.core.math.vertex.removeVertexAt!(MeshVertex*, (MeshVertex* i){ vertices = vertices.removeByValue(i); })(vertices, vertex, incViewportZoom);
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
        ulong off = vertices.length;
        if (isOnMirror(mousePos, meshEditAOE)) {
            placeOnMirror(mousePos, meshEditAOE);
        } else {
            foreachMirror((uint axis) {
                substituteMeshVertices(new MeshVertex(mirror(axis, mousePos)));
            });
        }
        refreshMesh();
        vertexMapDirty = true;
        if (io.KeyCtrl) selectOne(vertices.length - 1);
        else selectOne(off);
        return true;
    }

    override
    bool updateChanged(bool changed) {
        if (changed)
            this.changed = true;

        /*
        if (this.changed) {
            if (previewingTriangulation())
                previewMesh = mesh.autoTriangulate();
            this.changed = false;
        }
        */
        return changed;
    }

    override
    void addMeshVertex(MeshVertex* v2) {
        vertices ~= v2;
    }

    override
    int indexOfMesh(MeshVertex* v2) {
        return cast(int)vertices.countUntil(v2);
    }

    override
    void insertMeshVertex(int index, MeshVertex* v2) { 
        vertices.insertInPlace(index, v2);
    }

    override
    void removeMeshVertex(MeshVertex* v2) {
        vertices = vertices.removeByValue(v2);
    }

    override
    void moveMeshVertex(MeshVertex* v, vec2 newPos) {
        v.position = newPos;
    }

    override
    bool isPointOver(vec2 mousePos) {
        return isPointOverVertex(vertices, mousePos, incViewportZoom);
    }

    override
    ulong[] getInRect(vec2 min, vec2 max, uint groupId = 0) { 
        return nijigenerate.core.math.vertex.getInRect(vertices, selectOrigin, mousePos, groupId);
    }

    override
    void createPathTarget() {
    }

    override
    mat4 updatePathTarget() {
        return mat4.identity;
    }

    override
    void resetPathTarget() {
    }

    override
    void remapPathTarget(ref CatmullSpline p, mat4 trans) {
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

        vec3[] points;
        points ~= vec3(0, 0, 0);
        inDbgSetBuffer(points);
        inDbgPointsSize(10);
        inDbgDrawPoints(vec4(0, 0, 0, 1), trans);
        inDbgPointsSize(6);
        inDbgDrawPoints(vec4(0.5, 1, 0.5, 1), trans);

        points.length = vertices.length;
        foreach (i; 0..vertices.length) {
            points[i] = vec3(vertices[i].position, 0);
        }
        if (points.length > 0) {
            if (auto deformable = cast(PathDeformer)target) {
                /**
                    Draws the mesh
                */
                void drawLines(Curve curve, mat4 trans = mat4.identity, vec4 color = vec4(0.5, 1, 0.5, 1)) {
                    if (curve.controlPoints.length == 0)
                        return;
                    vec3[] lines;
                    foreach (i; 1..100) {
                        lines ~= vec3(curve.point((i - 1) / 100.0), 0);
                        lines ~= vec3(curve.point(i / 100.0), 0);
                    }
                    if (lines.length > 0) {
                        inDbgSetBuffer(lines);
                        inDbgDrawLines(color, trans);
                    }
                }
                auto curve = deformable.createCurve(vertices.map!((v)=>v.position).array);
                drawLines(curve, trans, edgeColor);
            }
            inDbgSetBuffer(points);
            inDbgPointsSize(10);
            inDbgDrawPoints(vec4(0, 0, 0, 1), trans);
            inDbgPointsSize(6);
            inDbgDrawPoints(vertexColor, trans);
        }

        if (vtxAtMouse != ulong(-1) && !isSelecting) {
            MeshVertex*[] one = getVerticesByIndex([vtxAtMouse], true);
            drawPointSubset(one, vec4(1, 1, 1, 0.3), trans, 15);
        }

        if (selected.length) {
            if (isSelecting && !mutateSelection) {
                auto selectedVertices = getVerticesByIndex(selected, true);
                drawPointSubset(selectedVertices, vec4(0.6, 0, 0, 1), trans);
            }
            else {
                auto selectedVertices = getVerticesByIndex(selected, true);
                drawPointSubset(selectedVertices, vec4(1, 0, 0, 1), trans);
            }
        }

        if (mirrorSelected.length) {
            auto mirroredVertices = getVerticesByIndex(mirrorSelected, true);
            drawPointSubset(mirroredVertices, vec4(1, 0, 1, 1), trans);
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
                    drawPointSubset(newSelectedVertices, vec4(1, 0, 1, 1), trans);
                }
                else {
                    auto newSelectedVertices = getVerticesByIndex(newSelected, true);
                    drawPointSubset(newSelectedVertices, vec4(1, 0, 0, 1), trans);
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
