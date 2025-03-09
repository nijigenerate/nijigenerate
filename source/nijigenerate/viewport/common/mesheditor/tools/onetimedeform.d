module nijigenerate.viewport.common.mesheditor.tools.onetimedeform;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.vertex.mesheditor;
import nijigenerate.viewport.model.mesheditor;
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
import std.algorithm.searching;

enum SubToolMode {
    Select,
    Vertex,
    Deform
}
class OneTimeDeform(T) : NodeSelect {
public:
    T filter;
    IncMeshEditorOne fVertImpl;
    IncMeshEditorOne fDefImpl;
    int vertActionId;
    int defActionId;
    SubToolMode mode;

    enum OneTimeDeformActionID {
        SwitchMode = cast(int)(SelectActionID.End),
        End
    }

    override
    bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
        switch (mode) {
        case SubToolMode.Select:
            return super.onDragStart(mousePos, impl);
        case SubToolMode.Vertex:
            if (auto draggable = cast(Draggable)fVertImpl.getTool())
                return draggable.onDragStart(mousePos, fVertImpl);
            break;
        case SubToolMode.Deform:
            if (auto draggable = cast(Draggable)fDefImpl.getTool())
                return draggable.onDragStart(mousePos, fDefImpl);
            break;
        default:
            break;
        }
        return false;
    }

    override
    bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl) {
        switch (mode) {
        case SubToolMode.Select:
            return super.onDragUpdate(mousePos, impl);
        case SubToolMode.Vertex:
            if (auto draggable = cast(Draggable)fVertImpl.getTool())
                return draggable.onDragUpdate(mousePos, fVertImpl);
            break;
        case SubToolMode.Deform:
            if (auto draggable = cast(Draggable)fDefImpl.getTool())
                return draggable.onDragUpdate(mousePos, fDefImpl);
            break;
        default:
            break;
        }
        return false;
    }

    override
    bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl) {
        switch (mode) {
        case SubToolMode.Select:
            return super.onDragEnd(mousePos, impl);
        case SubToolMode.Vertex:
            if (auto draggable = cast(Draggable)fVertImpl.getTool())
                return draggable.onDragEnd(mousePos, fVertImpl);
            break;
        case SubToolMode.Deform:
            if (auto draggable = cast(Draggable)fDefImpl.getTool())
                return draggable.onDragEnd(mousePos, fDefImpl);
            break;
        default:
            break;
        }
        return false;
    }

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        super.setToolMode(toolMode, impl);
        if (!filter || incActivePuppet() != filter.puppet) {
            filter = new T(incActivePuppet().root);
            fVertImpl = new IncMeshEditorOneFor!(T, EditMode.VertexEdit);
            fVertImpl.vertexColor = vec4(0, 1, 1, 1);
            fVertImpl.edgeColor   = vec4(0, 1, 1, 1);
            fVertImpl.setTarget(filter);
            vertActionId = SelectActionID.None;

            fDefImpl  = new IncMeshEditorOneFor!(T, EditMode.ModelEdit);
            fDefImpl.vertexColor = vec4(0, 1, 0, 1);
            fDefImpl.edgeColor   = vec4(0, 1, 0, 1);
            fDefImpl.setTarget(filter);
            defActionId = SelectActionID.None;
        }
        if (filter.children.countUntil(impl.getTarget()) < 0) {
            filter.captureTarget(impl.getTarget());
        }
        mode = SubToolMode.Vertex;
    }

    override 
    int peek(ImGuiIO* io, IncMeshEditorOne impl) {
        if (incInputIsKeyPressed(ImGuiKey.Tab)) {
            return OneTimeDeformActionID.SwitchMode;
        }

        switch (mode) {
        case SubToolMode.Select:
            return super.peek(io, impl);
        case SubToolMode.Vertex:
            vertActionId = fVertImpl.getTool().peek(io, fVertImpl);
            break;
        case SubToolMode.Deform:
            defActionId = fDefImpl.getTool().peek(io, fDefImpl);
            break;
        default:
        }

        return SelectActionID.None;
    }

    override
    int unify(int[] actions) {
        int[int] priorities;
        priorities[OneTimeDeformActionID.SwitchMode] = 0;
        priorities[SelectActionID.None]                 = 10;
        priorities[SelectActionID.SelectArea]           = 5;
        priorities[SelectActionID.ToggleSelect]         = 2;
        priorities[SelectActionID.SelectOne]            = 2;
        priorities[SelectActionID.MaybeSelectOne]       = 2;
        priorities[SelectActionID.StartDrag]            = 2;
        priorities[SelectActionID.SelectMaybeSelectOne] = 2;

        int action = SelectActionID.None;
        int curPriority = priorities[action];
        foreach (a; actions) {
            auto newPriority = priorities[a];
            if (newPriority < curPriority) {
                curPriority = newPriority;
                action = a;
            }
        }
        return action;
    }

    override bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        incStatusTooltip(_("Switch Mode"), _("TAB"));

        if (action == OneTimeDeformActionID.SwitchMode) {
            switch (mode) {
            case SubToolMode.Select:
                mode = SubToolMode.Vertex;
                break;
            case SubToolMode.Vertex:
                mode = SubToolMode.Deform;
                fVertImpl.applyToTarget();
                fDefImpl.setTarget(filter);
                break;
            case SubToolMode.Deform:
                mode = SubToolMode.Vertex;
                break;
            default:
            }
            return true;
        }
        switch (mode) {
        case SubToolMode.Select:
            if (action != 0)
                writefln(" Select: %s", action);
            return super.update(io, impl, action, changed);
        case SubToolMode.Vertex:
            bool result = fVertImpl.getTool().update(io, fVertImpl, vertActionId, changed);
            return result;

        case SubToolMode.Deform:
            bool result = fDefImpl.getTool().update(io, fDefImpl, defActionId, changed);
            auto parameter = incArmedParameter();
            auto deform = cast(DeformationParameterBinding)parameter.getOrAddBinding(filter, "deform");
            deform.update(parameter.findClosestKeypoint(), fDefImpl.getOffsets());
            return result;
        default:
        }
        return false;
    }

    override
    void draw(Camera camera, IncMeshEditorOne impl) {
        switch (mode) {
        case SubToolMode.Select:
            return super.draw(camera, impl);
        case SubToolMode.Vertex:
            fVertImpl.draw(camera);
            return fVertImpl.getTool().draw(camera, fVertImpl);
        case SubToolMode.Deform:
            fDefImpl.draw(camera);
            return fDefImpl.getTool().draw(camera, fDefImpl);
        default:
        }

    }

    override
    MeshEditorAction!DeformationAction editorAction(Node target, DeformationAction action) {
        switch (mode) {
        case SubToolMode.Select:
            return super.editorAction(target, action);
        case SubToolMode.Vertex:
            return fVertImpl.getTool().editorAction(target, action);
        case SubToolMode.Deform:
            return fDefImpl.getTool().editorAction(target, action);
        default:
        }
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    override
    MeshEditorAction!GroupAction editorAction(Node target, GroupAction action) {
        switch (mode) {
        case SubToolMode.Select:
            return super.editorAction(target, action);
        case SubToolMode.Vertex:
            return fVertImpl.getTool().editorAction(target, action);
        case SubToolMode.Deform:
            return fDefImpl.getTool().editorAction(target, action);
        default:
        }
        return new MeshEditorAction!(GroupAction)(target, action);
    }

    override
    MeshEditorAction!DeformationAction editorAction(Drawable target, DeformationAction action) {
        switch (mode) {
        case SubToolMode.Select:
            return super.editorAction(target, action);
        case SubToolMode.Vertex:
            return fVertImpl.getTool().editorAction(target, action);
        case SubToolMode.Deform:
            return fDefImpl.getTool().editorAction(target, action);
        default:
        }
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    override
    MeshEditorAction!GroupAction editorAction(Drawable target, GroupAction action) {
        switch (mode) {
        case SubToolMode.Select:
            return super.editorAction(target, action);
        case SubToolMode.Vertex:
            return fVertImpl.getTool().editorAction(target, action);
        case SubToolMode.Deform:
            return fDefImpl.getTool().editorAction(target, action);
        default:
        }
        return new MeshEditorAction!(GroupAction)(target, action);
    }
}

class ToolInfoImpl(T: OneTimeDeform!MeshGroup) : ToolInfoBase!(T) {
    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (deformOnly)
            return super.viewportTools(deformOnly, toolMode, editors);
        return false;
    }
    override VertexToolMode mode() { return VertexToolMode.AltMeshGroup; };
    override string icon() { return "";}
    override string description() { return _("Mesh deformation");}
}

class ToolInfoImpl(T: OneTimeDeform!PathDeformer) : ToolInfoBase!(T) {
    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (deformOnly)
            return super.viewportTools(deformOnly, toolMode, editors);
        return false;
    }
    override VertexToolMode mode() { return VertexToolMode.AltBezierDeform; };
    override string icon() { return "";}
    override string description() { return _("Path deformation");}
}