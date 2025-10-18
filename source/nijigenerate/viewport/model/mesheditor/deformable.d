module nijigenerate.viewport.model.mesheditor.deformable;

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
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.utils: removeByValue;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
//import std.stdio;
import std.range: enumerate, zip;
import std.algorithm : map, sort;
import std.algorithm.iteration : uniq;
import std.array;

/**
 * MeshEditor of Deformable for deformation operation.
 */
class IncMeshEditorOneFor(T: Deformable, EditMode mode: EditMode.ModelEdit): IncMeshEditorOneDeformable if (!is(T: Drawable)) {
protected:
    vec2[] deformation;
    vec2[] inputVertices;

    override
    void substituteMeshVertices(MeshVertex* meshVertex) {
    }
    MeshEditorAction!DeformationAction editorAction = null;
    void updateTarget() {
        transform = deformable.getDynamicMatrix();
        foreach (i, vert; deformable.vertices) {
            inputVertices[i] = vert + deformable.deformation[i]; // FIXME: should handle origin
        }
    }

    void importDeformation() {
        Deformable drawable = cast(Deformable)target;
        if (drawable is null)
            return;
        auto param = incArmedParameter();
        auto binding = cast(DeformationParameterBinding)(param? param.getBinding(drawable, "deform"): null);
        if (binding is null) {
            deformation = drawable.deformation.dup;
        } else {
            auto deform = binding.getValue(param.findClosestKeypoint());
            if (drawable.deformation.length == deform.vertexOffsets.length) {
                deformation.length = drawable.deformation.length;
                foreach (i, d; drawable.deformation) {
                    deformation[i] = d - deform.vertexOffsets[i];
                }
            }
        }            
    }

public:

    this() {
        super(true);
    }

    override
    void setTarget(Node target) {
        Deformable defromable = cast(Deformable)target;
        if (defromable is null) {
            return;
        }
        importDeformation();
        super.setTarget(target);
        inputVertices = deformable.vertices.dup;
        vertices.resize(inputVertices.length);
        foreach (i; 0..inputVertices.length) {
            vertices[i].position = inputVertices[i];
        }
        updateTarget();
        refreshMesh();
    }

    override
    void resetMesh() {
        vertices = target.vertices.toMVertices;
    }

    override
    void refreshMesh() {
//        vertices = target.vertices.toMVertices;
        updateMirrorSelected();
    }

    override
    void importMesh(ref MeshData data) {
        vertices = target.vertices.toMVertices;
    }

    override
    void mergeMesh(ref MeshData data, mat4 matrix) {
        vertices ~= target.vertices.toMVertices;
    }

    override
    void applyOffsets(vec2[] offsets) {
        foreach(v,o; zip(vertices, offsets)) {
            v.position += o;
        }
    }

    override
    vec2[] getOffsets() {
        vec2[] offsets;

        offsets.length = vertices.length;
        foreach(idx, vertex; vertices) {
            if (idx < target.vertices.length)
                offsets[idx] = vertex.position - target.vertices[idx];
            else
                offsets[idx] = vertex.position;
        }
        return offsets;
    }

    override
    void applyToTarget() { }

    override
    void applyPreview() { }                      

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
        return nijigenerate.core.math.vertex.getVertexFromPoint(inputVertices, mousePos, incViewportZoom);
    }

    override
    float[] getVerticesInBrush(vec2 mousePos, Brush brush) {
        return nijigenerate.core.math.vertex.getVerticesInBrush(inputVertices, mousePos, brush);
    }

    override
    void removeVertexAt(vec2 vertex) { }

    override
    bool removeVertex(ImGuiIO* io, bool selectedOnly) { return false; }

    override
    bool addVertex(ImGuiIO* io) { return false; }

    override
    bool updateChanged(bool changed) { return changed; }

    override
    void addMeshVertex(MeshVertex* v2) { }

    override
    void insertMeshVertex(int index, MeshVertex* v2) { }

    override
    void removeMeshVertex(MeshVertex* v2) { }

    override
    void moveMeshVertex(MeshVertex* v, vec2 newPos) {
        v.position = newPos;
    }

    override
    bool isPointOver(vec2 mousePos) {
        return nijigenerate.core.math.vertex.isPointOverVertex(inputVertices, mousePos, incViewportZoom);
    }

    override
    ulong[] getInRect(vec2 min, vec2 max, uint groupId) {
        if (min.x > max.x) swap(min.x, max.x);
        if (min.y > max.y) swap(min.y, max.y);

        ulong[] matching;
        foreach(idx, vertex; inputVertices) {
            if (min.x > vertex.position.x) continue;
            if (min.y > vertex.position.y) continue;
            if (max.x < vertex.position.x) continue;
            if (max.y < vertex.position.y) continue;
            matching ~= idx;
        }

        return matching;        
    }

    override
    ulong[] filterVertices(bool delegate(MeshVertex*) filter) {
        ulong[] matching;
        MeshVertex mv;
        foreach (idx, vertex; inputVertices) {
            mv.position = vertex;
            if (filter(&mv)) matching ~= idx;
        }

        return matching;
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
        auto deformable = cast(Deformable)target;
        updateTarget();
        auto trans = transform;

        vec3[] points;

        points ~= vec3(0, 0, 0);
        inDbgSetBuffer(points);
        inDbgPointsSize(10);
        inDbgDrawPoints(vec4(0, 0, 0, 1), trans);
        inDbgPointsSize(6);
        inDbgDrawPoints(vec4(0.5, 1, 0.5, 1), trans);

        points.length = inputVertices.length;
        foreach (i; 0..inputVertices.length) {
            points[i] = vec3(inputVertices[i].position, 0);
        }
        if (points.length > 0) {
            if (auto deformer = cast(PathDeformer)target) {
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
                auto curve = deformer.createCurve(inputVertices.map!((v)=>v).array);
                drawLines(curve, trans, edgeColor);
            } else if (auto grid = cast(GridDeformer)target) {
                auto baseVerts = grid.vertices;
                if (baseVerts.length >= 4) {
                    auto xs = baseVerts.map!(v => v.x).array;
                    auto ys = baseVerts.map!(v => v.y).array;
                    xs.sort();
                    ys.sort();
                    xs = xs.uniq.array;
                    ys = ys.uniq.array;
                    size_t cols = xs.length;
                    size_t rows = ys.length;
                    if (cols >= 2 && rows >= 2 && cols * rows == baseVerts.length) {
                        vec3[] lines;
                        bool haveDeform = grid.deformation.length == baseVerts.length;
                        foreach (y; 0 .. rows) {
                            foreach (x; 0 .. cols) {
                                size_t idx = y * cols + x;
                                vec2 startPos = baseVerts[idx];
                                if (haveDeform) startPos += grid.deformation[idx];
                                auto start = vec3(startPos, 0);
                                if (x + 1 < cols) {
                                    size_t nextIdx = idx + 1;
                                    vec2 rightPos = baseVerts[nextIdx];
                                    if (haveDeform) rightPos += grid.deformation[nextIdx];
                                    lines ~= start;
                                    lines ~= vec3(rightPos, 0);
                                }
                                if (y + 1 < rows) {
                                    size_t nextIdx = idx + cols;
                                    vec2 downPos = baseVerts[nextIdx];
                                    if (haveDeform) downPos += grid.deformation[nextIdx];
                                    lines ~= start;
                                    lines ~= vec3(downPos, 0);
                                }
                            }
                        }
                        if (lines.length > 0) {
                            inDbgSetBuffer(lines);
                            inDbgDrawLines(edgeColor, trans);
                        }
                    }
                }
            }
            inDbgSetBuffer(points);
            inDbgPointsSize(10);
            inDbgDrawPoints(vec4(0, 0, 0, 1), trans);
            inDbgPointsSize(6);
            inDbgDrawPoints(vertexColor, trans);
        }

        if (vtxAtMouse != ulong(-1) && !isSelecting) {
            if (vtxAtMouse < inputVertices.length && vtxAtMouse >= 0)
                drawPointSubset([inputVertices[vtxAtMouse]], vec4(1, 1, 1, 0.3), trans, 15);
        }

        if (selected.length) {
            if (isSelecting && !mutateSelection) {
                auto selectedVertices = selected.map!((i) => i < inputVertices.length ? inputVertices[i]: vec2.init).array;
                if (selectedVertices.length > 0)
                    drawPointSubset(selectedVertices, vec4(0.6, 0, 0, 1), trans);
            }
            else {
                auto selectedVertices = selected.map!((i) => i < inputVertices.length ? inputVertices[i]: vec2.init).array;
                if (selectedVertices.length > 0)
                    drawPointSubset(selectedVertices, vec4(1, 0, 0, 1), trans);
            }
        }

        if (mirrorSelected.length) {
            auto mirrorSelectedVertices = mirrorSelected.map!((i) => i < inputVertices.length ? inputVertices[i]: vec2.init).array;
            drawPointSubset(mirrorSelectedVertices, vec4(1, 0, 1, 1), trans);
        }

        if (isSelecting) {
            vec3[] rectLines = incCreateRectBuffer(selectOrigin, mousePos);
            inDbgSetBuffer(rectLines);
            if (!mutateSelection) inDbgDrawLines(vec4(1, 0, 0, 1), trans);
            else if(invertSelection) inDbgDrawLines(vec4(0, 1, 1, 0.8), trans);
            else inDbgDrawLines(vec4(0, 1, 0, 0.8), trans);

            if (newSelected.length) {
                auto newSelectedVertices = newSelected.map!((i) => i < inputVertices.length ? inputVertices[i]: vec2.init).array;
                if (mutateSelection && invertSelection)
                    drawPointSubset(newSelectedVertices, vec4(1, 0, 1, 1), trans);
                else
                    drawPointSubset(newSelectedVertices, vec4(1, 0, 0, 1), trans);
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
    }

}
