module nijigenerate.viewport.common.mesheditor.tools.base;

import i18n;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.core.input;
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
//import std.stdio;
import std.string: toStringz;

class Tool {
    abstract int peek(ImGuiIO* io, IncMeshEditorOne impl);
    abstract int unify(int[] actions);
    abstract bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed);
    abstract void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl);
    abstract void draw(Camera camera, IncMeshEditorOne impl);
    abstract void finalizeToolMode(IncMeshEditorOne impl);

    MeshEditorAction!DeformationAction editorAction(Node target, DeformationAction action) {
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    MeshEditorAction!GroupAction editorAction(Node target, GroupAction action) {
        return new MeshEditorAction!(GroupAction)(target, action);
    }

    MeshEditorAction!DeformationAction editorAction(Drawable target, DeformationAction action) {
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    MeshEditorAction!GroupAction editorAction(Drawable target, GroupAction action) {
        return new MeshEditorAction!(GroupAction)(target, action);
    }
}


interface Draggable {
    bool onDragStart(vec2 mousePos, IncMeshEditorOne impl);
    bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl);
    bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl);
}

/// ToolInfo holds meta information and UI handling code for tools.
/// Objects of this class are instantiated only once in the program, and stored into infoList array.
interface ToolInfo {
    /// setupToolMode is called when tool are selected and being active.
    /// This function setup all required setup for IncMeshEditorOne level.
    /// Originally implemented directly in IncMeshEditorOneImpl, but move here
    /// in order to decouple tools and oprations.
    void setupToolMode(IncMeshEditorOne e, VertexToolMode mode);

    /// viewportTools is called from MeshEditor, and displays tool icons
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors);

    /// viewportTools is called from MeshEditor, and displays options for tool
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors);

    /// icon returns material font strings for tool.
    string icon();

    /// description returns tooltip text for tool.
    string description();

    /// mode return VertexToolMode for tool. Should be removed in the future.
    VertexToolMode mode();

    /// newTool returns new instance of tool.
    Tool newTool();

    /// Whether this tool can be used in the current context.
    /// Defaults to true; specific ToolInfoImpl may restrict by mode or target types.
    bool canUse(bool deformOnly, Node[] targets);
}


/// Base implementation of ToolInfo interface.
/// Every instance of ToolInfo must inherit this class, and should be declared as ToolInfoImpl(class) template.
class ToolInfoBase(T) : ToolInfo {

    override
    void setupToolMode(IncMeshEditorOne e, VertexToolMode mode) {
        e.finalizeToolMode();
        e.setToolMode(mode);
        e.setPath(null);
        e.refreshMesh();
    }

    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        bool result = false;
        if (incButtonColored(icon.toStringz, ImVec2(0, 0), toolMode == mode ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
            result = true;
        }
        incTooltip(description);
        return result;
    }
    override
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) { 
        return false;
    }
    abstract VertexToolMode mode();
    abstract string icon();
    abstract string description();
    override
    Tool newTool() { return new T; }

    override bool canUse(bool deformOnly, Node[] targets) { return true; }
}
