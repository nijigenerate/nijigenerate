module nijigenerate.viewport.common.mesheditor.operations.impl;

import nijigenerate.viewport.common.mesheditor.tools;
import nijigenerate.viewport.common.mesheditor.operations.base;
import i18n;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.core.math.vertex;
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
//import std.stdio;
import std.range;
import std.algorithm;


class IncMeshEditorOneImpl(T) : IncMeshEditorOne {
protected:
    T target;
    uint groupId;

    Tool[VertexToolMode] tools;
    Node[] filterTargets;

public:
    this(bool deformOnly) {
        super(deformOnly);
        ToolInfo[] infoList = incGetToolInfo();
        foreach (info; infoList) {
            tools[info.mode()] = info.newTool();
        }
    }

    override
    Node getTarget() {
        return target;
    }

    // Default no-op implementations so non-drawable editors don't need to implement edge ops
    override void forEachEdge(void delegate(MeshVertex*, MeshVertex*) visitor) { }
    override MeshDisconnectAction newMeshDisconnectAction() {
        assert(0, "newMeshDisconnectAction is only supported for drawable editors");
        return null;
    }

    override
    void setTarget(Node target) {
        this.target = cast(T)(target);
    }

    override
    CatmullSpline getPath() {
        auto pathTool = cast(PathDeformTool)(tools[VertexToolMode.PathDeform]);
        return pathTool.path;
    }

    override
    void setPath(CatmullSpline path) {
        auto pathTool = cast(PathDeformTool)(tools[VertexToolMode.PathDeform]);
        pathTool.setPath(path);
    }

    override int peek(ImGuiIO* io, Camera camera) {
        if (toolMode in tools) {
            return tools[toolMode].peek(io, this);
        }
        assert(0);
    }

    override int unify(int[] actions) {
        if (toolMode in tools) {
            return tools[toolMode].unify(actions);
        }
        assert(0);
    }

    override
    bool update(ImGuiIO* io, Camera camera, int actions) {
        bool changed = false;
        if (toolMode in tools) {
            tools[toolMode].update(io, this, actions, changed);
        } else {
            assert(0);
        }

        if (isSelecting) {
            newSelected = getInRect(selectOrigin, mousePos, groupId);
            mutateSelection = io.KeyShift;
            invertSelection = io.KeyCtrl;
        }

        return updateChanged(changed);
    }

    override
    void setToolMode(VertexToolMode toolMode) {
        if (this.toolMode == toolMode) return;

        if (toolMode in tools) {
            abortToolMode();
            this.toolMode = toolMode;
            tools[toolMode].setToolMode(toolMode, this);
        }
    }

    override
    void finalizeToolMode() {
        if (toolMode in tools) {
            tools[toolMode].finalizeToolMode(this);
        }
    }

    override
    void abortToolMode() {
        if (toolMode in tools) {
            tools[toolMode].abortToolMode(this);
        }
    }


    override
    ulong selectOne(ulong vertIndex) {
        auto vertex = getVerticesByIndex([vertIndex]);
        if (groupId > 0 && vertex[0] !is null && vertex[0].groupId != groupId) {
            selected = [];
            return cast(ulong)-1;
        } else {
            return super.selectOne(vertIndex);
        }
    }

    override
    Tool getTool() { return tools[toolMode]; }

    override
    uint getGroupId() { return groupId; }
    override
    void setGroupId(uint groupId) { this.groupId = groupId; }

    override
    Node[] getFilterTargets() {
        return filterTargets;
    }

    override
    void addFilterTarget(Node parent) {
        if (!filterTargets.canFind(parent))
            filterTargets ~= parent;
    }

    override
    void removeFilterTarget(Node parent) {
        long idx = filterTargets.countUntil(parent);
        if (idx >= 0) {
            filterTargets = filterTargets.remove(idx);
        }
    }

}

Vec2Array getVertices(T)(T node) {
    if (auto deform = cast(Deformable)node) {
        return deform.vertices;
    }
    return Vec2Array([node.transform.translation.xy]);
}
void setVertices(T)(T node, Vec2Array value) {
    if (auto deform = cast(Deformable)node) {
        deform.vertices = value;
    } else {
        node.transform.translation = vec3(value[0], value[1], 0);
    }
}
Vec2Array toVertices(T: MeshVertex*)(T[] array) {
    return Vec2Array(array.map!((MeshVertex* vtx){return vtx.position; }).array);
}
MeshVertex*[] toMVertices(T: vec2)(T[] array) {
    return array.map!((vec2 vtx) { return new MeshVertex(vtx); }).array;
}

MeshVertex*[] toMVertices(Vec2Array array) {
    MeshVertex*[] result;
    result.reserve(array.length);
    foreach (i; 0 .. array.length) {
        result ~= new MeshVertex(array[i].toVector());
    }
    return result;
}

void resize(T:MeshVertex*)(ref T[] array, ulong size) {
    if (size <= array.length) {
        array.length = size;
    } else {
        auto missing = size - array.length;
        while (missing != 0) {
            array ~= new MeshVertex;
            missing--;
        }
    }
}

bool toBool(T: MeshVertex*)(T vtx) { return vtx !is null; }
bool toBool(T: vec2)(T vtx) { return true; }
void drawPointSubset(T)(T[] subset, vec4 color, mat4 trans = mat4.identity, float size=6) {
    Vec3Array subPoints;
    if (subset.length == 0) return;

    // Updates all point positions
    foreach(vtx; subset) {
        if (toBool(vtx))
            subPoints ~= vec3(vtx.position, 0);
    }
    inDbgSetBuffer(subPoints);
    inDbgPointsSize(size);
    inDbgDrawPoints(color, trans);
}


class IncMeshEditorOneDeformable : IncMeshEditorOneImpl!Deformable {
protected:
    bool changed;
public:
    MeshVertex*[] vertices;

    this(bool deformOnly) {
        super(deformOnly);
    }

    Deformable deformable() { return cast(Deformable)getTarget(); }

    override 
    MeshVertex*[] getVerticesByIndex(ulong[] indices, bool removeNull = false) {
        MeshVertex*[] result;
        foreach (idx; indices) {
            if (idx < vertices.length)
                result ~= vertices[idx];
            else if (!removeNull)
                result ~= null;
        }
        return result;
    }
}

private bool refiningWeldingMesh;

size_t ngRefineWeldingSeams(Drawable first, Drawable second) {
    import nijigenerate.core.math.welding : ngRefineWeldingMesh;
    import nijigenerate.core.math.mesh : applyMeshToTarget;
    import nijilive.math : Vec2Array, vec3u, vec4;
    if (refiningWeldingMesh || first is null || second is null || first is second) return 0;
    auto coarse = first.vertices.length <= second.vertices.length ? first : second;
    auto fine = coarse is first ? second : first;
    if (cast(Part)coarse is null || cast(Part)fine is null) return 0;
    vec2[] coarseWorld, coarseLocal, fineWorld;
    foreach (vertex; coarse.vertices) {
        coarseLocal ~= vertex;
        coarseWorld ~= (coarse.transform.matrix * vec4(vertex, 0, 1)).xy;
    }
    foreach (vertex; fine.vertices)
        fineWorld ~= (fine.transform.matrix * vec4(vertex, 0, 1)).xy;
    auto refinement = ngRefineWeldingMesh(fineWorld, fine.getMesh().indices,
        coarseWorld, coarseLocal, coarse.getMesh().indices, coarse.transform.matrix.inverse);
    if (!refinement.addedVertices) return 0;
    refiningWeldingMesh = true;
    scope(exit) refiningWeldingMesh = false;
    auto mesh = new IncMesh(coarse.getMesh());
    mesh.vertices.length = 0;
    vec3u[] triangles;
    for (size_t i = 0; i < refinement.indices.length; i += 3)
        triangles ~= vec3u(refinement.indices[i], refinement.indices[i + 1], refinement.indices[i + 2]);
    mesh.importVertsAndTris(Vec2Array(refinement.vertices), triangles);
    foreach (i; 0 .. coarse.vertices.length) mesh.vertices[i].originalIndex = i;
    mesh.refresh();
    // Resample existing keys; adding a stitch does not edit the authored bone motion.
    applyMeshToTarget(coarse, mesh.vertices, &mesh, true);
    return refinement.addedVertices;
}

void incUpdateWeldedPoints(Drawable drawable, const(ptrdiff_t)[] remap) {
    import nijigenerate.core.math.welding : ngRemapWeldingLinks;
    foreach (welded; drawable.welded.dup) {
        auto counter = welded.target.welded.countUntil!(a => a.target == drawable);
        if (counter < 0) continue;
        auto updated = ngRemapWeldingLinks(welded.indices, welded.target.welded[counter].indices,
            remap, drawable.vertices.length);
        incActionPush(new DrawableChangeWeldingAction(drawable, welded.target,
            updated.forward, welded.weight, updated.reverse));
    }
}

class IncMeshEditorOneDrawable : IncMeshEditorOneImpl!Drawable {
protected:
public:
    IncMesh mesh;

    this(bool deformOnly) {
        super(deformOnly);
    }

    ref IncMesh getMesh() {
        return mesh;
    }

    void setMesh(IncMesh mesh) {
        this.mesh = mesh;
    }

    MeshVertex*[] vertices() {
        return mesh.vertices;
    }

    // Provide safe helpers so tools don't access mesh directly
    override void forEachEdge(void delegate(MeshVertex*, MeshVertex*) visitor) {
        foreach (v; mesh.vertices) {
            // duplicate connections for safe iteration
            auto conns = v.connections.dup;
            foreach (v2; conns) {
                // Prevent duplicate visits; assumes mesh adjacency is symmetric.
                // TODO: consider comparing vertex IDs instead of pointers (currently unavailable).
                // Unique vertex pointers are assumed; integrity guaranteed during initialization/connection.
                debug assert(cast(void*) v2 != cast(void*) v);
                if (cast(void*) v2 < cast(void*) v)
                    continue;

                visitor(v, v2);
            }
        }
    }

    override MeshDisconnectAction newMeshDisconnectAction() {
        return new MeshDisconnectAction(this.getTarget().name, this, mesh);
    }
}


IncMeshEditorOne ngGetEditorFor(Node target) {
    import nijigenerate.viewport.model.deform;
    import nijigenerate.viewport.common.mesheditor;

    IncMeshEditor editor = incViewportModelDeformGetEditor();
    IncMeshEditorOne targetEditor = editor? editor.getEditorFor(target): null;
    if (targetEditor is null) {
        targetEditor = ngGetArmedParameterEditorFor(target);
    }
    return targetEditor;
}
