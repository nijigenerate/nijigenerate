module nijigenerate.viewport.common.mesheditor.operations.deformable;

import i18n;
import nijigenerate.viewport;
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

private {
    vec2[] getVertices(T)(T node) {
        if (auto deform = cast(Deformable)node) {
            return deform.vertices;
        }
        return [node.transform.translation.xy];
    }
    void setVertices(T)(T node, vec2[] value) {
        if (auto deform = cast(Deformable)node) {
            deform.vertices = value;
        } else {
            node.transform.translation = vec3(value[0], value[1], 0);
        }
    }
}


class IncMeshEditorOneDeformable : IncMeshEditorOneImpl!Deformable {
protected:
    bool changed;
public:
    vec2[] vertices;

    this(bool deformOnly) {
        super(deformOnly);
    }

    Deformable deformable() { return cast(Deformable)getTarget(); }
}

/**
 * MeshEditor of Deformable for vertex operation.
 */
class IncMeshEditorOneDeformableVertex : IncMeshEditorOneDeformable {
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
        if (deformable is null)
            return;
        super.setTarget(target);
        this.vertices = getTarget().getVertices();
        transform = target ? target.transform.matrix : mat4.identity;
    }

    override
    void resetMesh() {
        this.vertices = getTarget().getVertices();
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
        this.vertices ~= data.vertices;
    }

    override
    void mergeMesh(ref MeshData data, mat4 matrix) {
        this.vertices ~= data.vertices;
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
        incActionPushGroup();
        
        // Apply the model
        auto action = new DeformableChangeAction(target.name, target);

        // Export mesh
        //MeshData data = mesh.export_();
        //data.fixWinding();

        if (vertices.length != target.vertices.length)
            vertexMapDirty = true;

        DeformationParameterBinding[] deformers;

        void alterDeform(ParameterBinding binding) {
            auto deformBinding = cast(DeformationParameterBinding)binding;
            if (!deformBinding)
                return;
            foreach (uint x; 0..cast(uint)deformBinding.values.length) {
                foreach (uint y; 0..cast(uint)deformBinding.values[x].length) {
                    auto deform = deformBinding.values[x][y];
                    if (deformBinding.isSet(vec2u(x, y))) {
                        auto newDeform = deformByDeformationBinding(vertices, deformBinding, vec2u(x, y), false);
                        if (newDeform) 
                            deformBinding.values[x][y] = *newDeform;
                    } else {
                        deformBinding.values[x][y].vertexOffsets.length = this.vertices.length;
                    }
                    deformers ~= deformBinding;
                }
            }
        }

        foreach (param; incActivePuppet().parameters) {
            if (auto group = cast(ExParameterGroup)param) {
                foreach(x, ref xparam; group.children) {
                    ParameterBinding binding = xparam.getBinding(target, "deform");
                    if (binding)
                        action.addAction(new ParameterChangeBindingsAction("Deformation recalculation on mesh update", xparam, null));
                    alterDeform(binding);
                }
            } else {
                ParameterBinding binding = param.getBinding(target, "deform");
                if (binding)
                    action.addAction(new ParameterChangeBindingsAction("Deformation recalculation on mesh update", param, null));
                alterDeform(binding);
            }
        }
        incActivePuppet().resetDrivers();
        vertexMapDirty = false;
        
        target.clearCache();
        target.rebuffer(vertices);

        // reInterpolate MUST be called after rebuffer is called.
        foreach (deformBinding; deformers) {
            deformBinding.reInterpolate();
        }

        action.updateNewState();
        incActionPush(action);

        incActionPopGroup();
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
        return nijigenerate.core.math.vertex.getVertexFromPoint(vertices, mousePos);
    }

    override
    float[] getVerticesInBrush(vec2 mousePos, Brush brush) {
        return nijigenerate.core.math.vertex.getVerticesInBrush(vertices, mousePos, brush);
    }

    override
    void removeVertexAt(vec2 vertex) {
        nijigenerate.core.math.vertex.removeVertexAt!(vec2, (vec2 i){ vertices = vertices.removeByValue(i); })(vertices, vertex);
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
    void removeMeshVertex(MeshVertex* v2) {
        vertices = vertices.removeByValue(v2.position);
    }

    override
    bool isPointOver(vec2 mousePos) {
        return isPointOverVertex(vertices, mousePos);
    }

    override
    ulong[] getInRect(vec2 min, vec2 max, uint groupId = 0) { 
        return nijigenerate.core.math.vertex.getInRect(vertices, selectOrigin, mousePos, groupId);
    }
    override 
    MeshVertex*[] getVerticesByIndex(ulong[] indices, bool removeNull = false) {
        MeshVertex*[] result;
        foreach (idx; indices) {
            if (idx < vertices.length)
                result ~= new MeshVertex(vertices[idx]);
            else if (!removeNull)
                result ~= null;
        }
        return result;
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
        /*
        mat4 trans = mat4.identity;

        if (vtxAtMouse != ulong(-1) && !isSelecting) {
            MeshVertex*[] one = getVerticesByIndex([vtxAtMouse], true);
            mesh.drawPointSubset(one, vec4(1, 1, 1, 0.3), trans, 15);
        }

        if (previewMesh) {
            previewMesh.drawLines(trans, vec4(0.7, 0.7, 0, 1));
            mesh.drawPoints(trans);
        } else {
            mesh.draw(trans);
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
        */
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


/**
 * MeshEditor of Deformable for deformation operation.
 */
class IncMeshEditorOneDeformableDeform : IncMeshEditorOneDeformable {
protected:
    override
    void substituteMeshVertices(MeshVertex* meshVertex) {
    }
    MeshEditorAction!DeformationAction editorAction = null;
    void updateTarget() {
        auto drawable = cast(Deformable)target;
        transform = drawable.getDynamicMatrix();
        vertices.length = drawable.vertices.length;
        foreach (i, vert; drawable.vertices) {
            vertices[i] = drawable.vertices[i] + drawable.deformation[i]; // FIXME: should handle origin
        }
    }

    void importDeformation() {
        Deformable drawable = cast(Deformable)target;
        if (drawable is null)
            return;
        auto param = incArmedParameter();
        auto binding = cast(DeformationParameterBinding)(param? param.getBinding(drawable, "deform"): null);
        vec2[] deformation;
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
        foreach (i, d; deformation) {
            vertices[i] += deformation[i];
        }
            
    }

public:

    this() {
        super(true);
    }

    override
    void setTarget(Node target) {
        Deformable drawable = cast(Deformable)target;
        if (drawable is null)
            return;
        importDeformation();
        super.setTarget(target);
        updateTarget();
        refreshMesh();
    }

    override
    void resetMesh() {
        vertices = target.vertices[];
    }

    override
    void refreshMesh() {
        vertices = target.vertices[];
        updateMirrorSelected();
    }

    override
    void importMesh(ref MeshData data) {
        vertices = target.vertices[];
    }

    override
    void mergeMesh(ref MeshData data, mat4 matrix) {
        vertices ~= target.vertices;
    }

    override
    void applyOffsets(vec2[] offsets) {
//        mesh.applyOffsets(offsets);
    }

    override
    vec2[] getOffsets() {
        // TBD: Need confirmation.
        vec2[] offsets;

        offsets.length = vertices.length;
        foreach(idx, vertex; vertices) {
            if (idx < target.vertices.length)
                offsets[idx] = vertex - target.vertices[idx];
            else
                offsets[idx] = vertex;
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
        return nijigenerate.core.math.vertex.getVertexFromPoint(vertices, mousePos);
    }

    override
    float[] getVerticesInBrush(vec2 mousePos, Brush brush) {
        return nijigenerate.core.math.vertex.getVerticesInBrush(vertices, mousePos, brush);
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
    void removeMeshVertex(MeshVertex* v2) { }

    override
    bool isPointOver(vec2 mousePos) {
        return nijigenerate.core.math.vertex.isPointOverVertex(vertices, mousePos);
    }

    override
    ulong[] getInRect(vec2 min, vec2 max, uint groupId) {
        if (min.x > max.x) swap(min.x, max.x);
        if (min.y > max.y) swap(min.y, max.y);

        ulong[] matching;
        foreach(idx, vertex; vertices) {
            if (min.x > vertex.x) continue;
            if (min.y > vertex.y) continue;
            if (max.x < vertex.x) continue;
            if (max.y < vertex.y) continue;
            matching ~= idx;
        }

        return matching;        
    }

    override 
    MeshVertex*[] getVerticesByIndex(ulong[] indices, bool removeNull = false) {
        MeshVertex*[] result;
        foreach (idx; indices) {
            if (idx < vertices.length)
                result ~= new MeshVertex(vertices[idx]);
            else if (!removeNull)
                result ~= null;
        }
        return result;
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
        auto drawable = cast(Deformable)target;
        updateTarget();
        auto trans = transform;
        /*
        MeshVertex*[] _getVerticesByIndex(ulong[] indices) {
            MeshVertex*[] result;
            foreach (idx; indices) {
                if (idx < vertices.length)
                    result ~= new MeshVertex(vertices[idx]);
            }
            return result;
        }

        drawable.drawMeshLines();
        vec3[] points;
        points.length = vertices.length;
        foreach (i; 0..vertices.length) {
            points[i] = vec3(vertices[i], 0);
        }
        if (points.length > 0) {
            inDbgSetBuffer(points);
            inDbgPointsSize(10);
            inDbgDrawPoints(vec4(0, 0, 0, 1), trans);
            inDbgPointsSize(6);
            inDbgDrawPoints(vec4(1, 1, 1, 1), trans);
        }

        if (groupId != 0) {
            MeshVertex*[] vertsInGroup = [];
            foreach (v; mesh.vertices) {
                if (v.groupId != groupId) vertsInGroup ~= v;
            }
            mesh.drawPointSubset(vertsInGroup, vec4(0.6, 0.6, 0.6, 1), trans);
        }

        if (vtxAtMouse != ulong(-1) && !isSelecting) {
            MeshVertex*[] one = _getVerticesByIndex([vtxAtMouse]);
            mesh.drawPointSubset(one, vec4(1, 1, 1, 0.3), trans, 15);
        }

        if (selected.length) {
            if (isSelecting && !mutateSelection) {
                auto selectedVertices = _getVerticesByIndex(selected);
                mesh.drawPointSubset(selectedVertices, vec4(0.6, 0, 0, 1), trans);
            }
            else {
                auto selectedVertices = _getVerticesByIndex(selected);
                mesh.drawPointSubset(selectedVertices, vec4(1, 0, 0, 1), trans);
            }
        }

        if (mirrorSelected.length) {
            auto mirrorSelectedVertices = _getVerticesByIndex(mirrorSelected);
            mesh.drawPointSubset(mirrorSelectedVertices, vec4(1, 0, 1, 1), trans);
        }

        if (isSelecting) {
            vec3[] rectLines = incCreateRectBuffer(selectOrigin, mousePos);
            inDbgSetBuffer(rectLines);
            if (!mutateSelection) inDbgDrawLines(vec4(1, 0, 0, 1), trans);
            else if(invertSelection) inDbgDrawLines(vec4(0, 1, 1, 0.8), trans);
            else inDbgDrawLines(vec4(0, 1, 0, 0.8), trans);

            if (newSelected.length) {
                auto newSelectedVertices = _getVerticesByIndex(newSelected);
                if (mutateSelection && invertSelection)
                    mesh.drawPointSubset(newSelectedVertices, vec4(1, 0, 1, 1), trans);
                else
                    mesh.drawPointSubset(newSelectedVertices, vec4(1, 0, 0, 1), trans);
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
        */

        if (toolMode in tools)
            tools[toolMode].draw(camera, this);
    }

    override
    void adjustPathTransform() {
    }

}