module nijigenerate.viewport.common.mesheditor.tools.onetimedeform;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import i18n;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
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
import std.stdio;

class OneTimeDeform(T) : Tool, Draggable {
public:
    T filter;
    IncMeshEditorOne fVertImpl;
    IncMeshEditorOne fDefImpl;

    override
    bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
    }

    override
    bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl) {
    }

    override
    bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl) {
    }

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        super.setToolMode(toolMode, impl);
        filter = new T(null);
        filter.setPuppet(incActivePuppet());
        fVertImpl = new IncMeshEditorOneFor!(T, EditMode.Vertex);
        fDefImpl  = new IncMeshEditorOneFor!(T, EditMode.Model);
    }

    override 
    int peek(ImGuiIO* io, IncMeshEditorOne impl) {
        super.peek(io, impl);
    }

    override
    int unify(int[] actions) {
    }

    override bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        bool result = super.update(io, impl, action, changed);
        return result;
    }

    override
    void draw(Camera camera, IncMeshEditorOne impl) {

    }

    override
    MeshEditorAction!DeformationAction editorAction(Node target, DeformationAction action) {
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    override
    MeshEditorAction!GroupAction editorAction(Node target, GroupAction action) {
        return new MeshEditorAction!(GroupAction)(target, action);
    }

    override
    MeshEditorAction!DeformationAction editorAction(Drawable target, DeformationAction action) {
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    override
    MeshEditorAction!GroupAction editorAction(Drawable target, GroupAction action) {
        return new MeshEditorAction!(GroupAction)(target, action);
    }
}
/+
class ToolInfoImpl(T) : ToolInfoBase!(OneTimeDeform) {
    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (!deformOnly)
            return super.viewportTools(deformOnly, toolMode, editors);
        return false;
    }
    override VertexToolMode mode() { return VertexToolMode.Connect; };
    override string icon() { return "";}
    override string description() { return _("Edge Tool");}
}
+/