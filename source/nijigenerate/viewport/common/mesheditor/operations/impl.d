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
import nijilive.core.dbg;
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

        if (this.toolMode in tools) {
            tools[this.toolMode].finalizeToolMode(this);
        }
        if (toolMode in tools) {
            this.toolMode = toolMode;
            tools[toolMode].setToolMode(toolMode, this);
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

}

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
vec2[] toVertices(T: MeshVertex*)(T[] array) {
    return array.map!((MeshVertex* vtx){return vtx.position; }).array;
}
MeshVertex*[] toMVertices(T: vec2)(T[] array) {
    return array.map!((vec2 vtx) { return new MeshVertex(vtx); }).array;
}

void resize(T:MeshVertex*)(ref T[] array, ulong size) {
    if (size <= array.length) {
        array.length = size;
    } else {
        while (size != 0) {
            array ~= new MeshVertex;
            size --;
        }
    }
}

bool toBool(T: MeshVertex*)(T vtx) { return vtx !is null; }
bool toBool(T: vec2)(T vtx) { return true; }
void drawPointSubset(T)(T[] subset, vec4 color, mat4 trans = mat4.identity, float size=6) {
    vec3[] subPoints;
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

void incUpdateWeldedPoints(Drawable drawable) {
    foreach (welded; drawable.welded) {
        ptrdiff_t[] indices;
        foreach (i, v; drawable.vertices) {
            auto vv = drawable.transform.matrix * vec4(v, 0, 1);
            auto minDistance = welded.target.vertices.enumerate.minElement!((a)=>(welded.target.transform.matrix * vec4(a.value, 0, 1)).distance(vv))();
            if ((welded.target.transform.matrix * vec4(minDistance[1], 0, 1)).distance(vv) < 4)
                indices ~= minDistance[0];
            else
                indices ~= -1;
        }
        incActionPush(new DrawableChangeWeldingAction(drawable, welded.target, indices, welded.weight));
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
}
