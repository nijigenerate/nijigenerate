module nijigenerate.viewport.common.mesheditor.operations.impl;

import nijigenerate.viewport.common.mesheditor.tools;
import nijigenerate.viewport.common.mesheditor.operations.base;
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
import nijilive;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
import std.stdio;


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