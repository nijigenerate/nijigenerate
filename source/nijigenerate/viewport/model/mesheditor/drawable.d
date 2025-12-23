module nijigenerate.viewport.model.mesheditor.drawable;

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
import nijigenerate.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
import std.algorithm;
//import std.stdio;
import std.range: enumerate;
import std.array;
import std.typecons;

/**
 * MeshEditor of Drawable for deformation operation.
 */
class IncMeshEditorOneFor(T: Drawable, EditMode mode: EditMode.ModelEdit) : IncMeshEditorOneDrawable {
protected:
    override
    void substituteMeshVertices(MeshVertex* meshVertex) {
    }
    MeshEditorAction!DeformationAction editorAction = null;
    void updateTarget() {
        auto drawable = cast(Drawable)target;
        transform = drawable.getDynamicMatrix();
        vertices.length = drawable.vertices.length;
        foreach (i, vert; drawable.vertices) {
            vertices[i] = drawable.vertices[i] + drawable.deformation[i]; // FIXME: should handle origin
        }
    }

    void importDeformation() {
        Drawable drawable = cast(Drawable)target;
        if (drawable is null)
            return;
        deformation = drawable.deformation.dup;
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
    Vec2Array deformation;
    Vec2Array vertices;

    this() {
        super(true);
    }

    override
    void setTarget(Node target) {
        Drawable drawable = cast(Drawable)target;
        if (drawable is null)
            return;
        importDeformation();
        super.setTarget(target);
        updateTarget();
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
    void applyOffsets(Vec2Array offsets) {
        mesh.applyOffsets(offsets);
    }

    override
    Vec2Array getOffsets() {
        return mesh.getOffsets();
    }

    override
    void applyToTarget() { }

    override
    void applyPreview() { }                      

    override
    void pushDeformAction() {
        import std.stdio;
        writefln("push deform action: %s, %s, %s", cast(void*)this, cast(void*)editorAction, target.name);
        if (editorAction && editorAction.action.dirty) {
            editorAction.updateNewState();
            incActionPush(editorAction);
            editorAction = null;
        }        
    }

    override
    ulong getVertexFromPoint(vec2 mousePos) {
        // return vertices position from mousePos
        foreach (i; 0 .. vertices.length) {
            vec2 vert = vertices[i].toVector();
            if (abs(vert.distance(mousePos)) < mesh.selectRadius / incViewportZoom)
                return i;
        }
        return -1;
    }

    override
    float[] getVerticesInBrush(vec2 mousePos, Brush brush) {
        return brush.weightsAt(mousePos, vertices);
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
    void addMeshVertex(MeshVertex* v2) {}

    override
    void insertMeshVertex(int index, MeshVertex* v2) {}

    override
    void removeMeshVertex(MeshVertex* v2) { }

    override
    void moveMeshVertex(MeshVertex* v, vec2 newPos) { v.position = newPos; }

    override
    bool isPointOver(vec2 mousePos) {
        foreach(vert; vertices) {
            if (abs(vert.distance(mousePos)) < mesh.selectRadius/incViewportZoom) return true;
        }
        return false;
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
    ulong[] filterVertices(bool delegate(MeshVertex*) filter) {
        ulong[] matching;
        MeshVertex mv;
        foreach (idx, vertex; vertices) {
            mv.position = vertex;
            if (filter(&mv)) matching ~= idx;
        }

        return matching;
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
        getPath().createTarget(mesh, mat4.identity, &vertices); //transform.inverse() * target.transform.matrix);
    }

    override
    mat4 updatePathTarget() {
        return getPath().updateTarget(mesh, selected, mat4.identity(), deformation);
    }

    override
    void resetPathTarget() {
        getPath().resetTarget(mesh);
    }

    override
    void remapPathTarget(ref CatmullSpline p, mat4 trans) {
        p.remapTarget(mesh, trans, &vertices); //mat4.identity);
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
//        editorAction = null;
    }

    override
    void draw(Camera camera) {
        auto drawable = cast(Drawable)target;
        updateTarget();
        auto trans = transform;

        MeshVertex*[] _getVerticesByIndex(ulong[] indices) {
            MeshVertex*[] result;
            foreach (idx; indices) {
                if (idx < vertices.length)
                    result ~= new MeshVertex(vertices[idx]);
            }
            return result;
        }

        drawable.drawMeshLines(edgeColor);
        Vec3Array points;
        points.length = vertices.length;
        foreach (i; 0..vertices.length) {
            points[i] = vec3(vertices[i].toVector(), 0);
        }
        if (points.length > 0) {
            inDbgSetBuffer(points);
            inDbgPointsSize(10);
            inDbgDrawPoints(vec4(0, 0, 0, 1), trans);

            Tuple!(ptrdiff_t[], vec4)[] markers;
            if (drawable) {
                if (drawable.welded) {
                    foreach (welded; drawable.welded) {
                        auto t = tuple(welded.indices.enumerate.filter!(i=>i[1]!=cast(ptrdiff_t)-1).map!((p)=>cast(ptrdiff_t)p[0]).array, vec4(0, 0.7, 0.7, 1));
                        if (t[0].length > 0)
                            markers ~= t;
                    }
                }
            }

            if (markers) {
                foreach (marker; markers) {
                    auto pts = marker[0].map!(i => points[i].toVector()).array;
                    inDbgSetBuffer(Vec3Array(pts));
                    inDbgPointsSize(10);
                    inDbgDrawPoints(marker[1], trans);
                }
            }

            inDbgSetBuffer(points);
            inDbgPointsSize(6);
            inDbgDrawPoints(vertexColor, trans);
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
            Vec3Array rectLines = incCreateRectBuffer(selectOrigin, mousePos);
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
        Vec3Array axisLines;
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
        auto drawable = cast(Drawable)target;

        mat4 trans = (target? drawable.getDynamicMatrix(): transform).inverse * transform;
        importDeformation();
        ref CatmullSpline doAdjust(ref CatmullSpline p) {
            p.update();

            remapPathTarget(p, trans);
            return p;
        }
        if (getPath()) {
            if (getPath().target)
                getPath().target = doAdjust(getPath().target);
            auto path = getPath();
            setPath(doAdjust(path));
        }
        lastMousePos = (trans * vec4(lastMousePos, 0, 1)).xy;
        updateTarget();

        forceResetAction();
    }

}
