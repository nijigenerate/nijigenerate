module nijigenerate.viewport.common.mesheditor.tools.onetimedeform;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.vertex.mesheditor;
import nijigenerate.viewport.model.mesheditor;
import nijigenerate.viewport.model.deform;
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
//import std.stdio;
import std.algorithm;
import std.array;

enum SubToolMode {
    Select,
    Vertex,
    Deform
}

private {
    NodeFilter filter = null;
    IncMeshEditorOne fVertImpl = null;
    IncMeshEditorOne fDefImpl = null;
    int vertActionId = 0;
    int defActionId  = 0;

    void forceFinalize() {
        if (filter) {
            foreach (child; (cast(Node)filter).children) {
                filter.applyDeformToChildren([incArmedParameter()], false);
                filter.releaseTarget(child);
            }
            if ((cast(Node)filter).children.length == 0) {
                (cast(Node)filter).reparent(null, 0);
                filter = null;
            }
            incViewportNodeDeformNotifyParamValueChanged();
        }        
    }

    bool initialize(T)() {
        if (filter is null || cast(T)filter is null) {
            if (filter)
                forceFinalize();
            filter = new T(incActivePuppet().root);
            fVertImpl = new IncMeshEditorOneFor!(T, EditMode.VertexEdit);
            fVertImpl.vertexColor = vec4(0, 1, 1, 1);
            fVertImpl.edgeColor   = vec4(0, 1, 1, 1);
            fVertImpl.setTarget(cast(T)filter);
            vertActionId = NodeSelect.SelectActionID.None;

            fDefImpl  = new IncMeshEditorOneFor!(T, EditMode.ModelEdit);
            fDefImpl.vertexColor = vec4(0, 1, 0, 1);
            fDefImpl.edgeColor   = vec4(0, 1, 0, 1);
            fDefImpl.setTarget(cast(T)filter);
            defActionId = NodeSelect.SelectActionID.None;
            setup!(T);
            return true;
        }
        return false;
    }

    void setup(T: MeshGroup)() {
        fVertImpl.setToolMode(VertexToolMode.Grid);
        fDefImpl.setToolMode(VertexToolMode.Points);
    }

    void setup(T: PathDeformer)() {
        fVertImpl.setToolMode(VertexToolMode.BezierDeform);
        fDefImpl.setToolMode(VertexToolMode.BezierDeform);
    }
}
class OneTimeDeform(T) : NodeSelect {
public:
    SubToolMode mode;
    SubToolMode prevMode = SubToolMode.Vertex;
    bool acquired = false;

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
            if (acquired)
                if (auto draggable = cast(Draggable)fVertImpl.getTool())
                    return draggable.onDragStart(mousePos, fVertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired)
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
            if (acquired)
                if (auto draggable = cast(Draggable)fVertImpl.getTool())
                    return draggable.onDragUpdate(mousePos, fVertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired)
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
            if (acquired)
                if (auto draggable = cast(Draggable)fVertImpl.getTool())
                    return draggable.onDragEnd(mousePos, fVertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired)
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
        acquired = initialize!T();
        if ((cast(T)filter).children.countUntil(impl.getTarget()) < 0) {
            filter.captureTarget(impl.getTarget());
        }
        impl.getDeformAction();
        incActionPushStack();
        mode = SubToolMode.Vertex;
    }

    override
    void finalizeToolMode(IncMeshEditorOne impl) {
        if (acquired) {
            incActionPopStack();
            forceFinalize();
            impl.pushDeformAction();
        }
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
            if (acquired)
                vertActionId = fVertImpl.getTool().peek(io, fVertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired)
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
        void paramValueChanged() {
            DeformationParameterBinding deform = null;
            auto parameter = incArmedParameter();
            if (auto deformable = cast(Deformable)fDefImpl.getTarget()) {
                deform = cast(DeformationParameterBinding)parameter.getBinding(deformable, "deform");
                if (deform !is null) {
                    auto binding = deform.getValue(parameter.findClosestKeypoint());
                    fDefImpl.applyOffsets(binding.vertexOffsets);
                }
            }
        }

        if (action == OneTimeDeformActionID.SwitchMode) {
           
            switch (mode) {
            case SubToolMode.Select:
                mode = SubToolMode.Vertex;
                break;
            case SubToolMode.Vertex:
                if (acquired) {
                    mode = SubToolMode.Deform;
                    fVertImpl.vertexColor = vec4(0, 0.5, 0.5, 1);
                    fVertImpl.edgeColor   = vec4(0, 0.5, 0.5, 0.5);
                    fDefImpl.vertexColor = vec4(0, 1, 0, 1);
                    fDefImpl.edgeColor   = vec4(0, 1, 0, 1);
                    import std.stdio;
                    incActionPushGroup();
                    fVertImpl.pushDeformAction();
                    fVertImpl.applyToTarget();
                    fDefImpl.setTarget(cast(T)filter);
                    fDefImpl.getCleanDeformAction();
                    /*
                    if (auto deformable = cast(Deformable)filter) {
                        auto parameter = incArmedParameter();
                        auto deform = cast(DeformationParameterBinding)parameter.getBinding(deformable, "deform");
                        if (deform !is null)
                            deform.update(parameter.findClosestKeypoint(), fDefImpl.getOffsets());
                    }
                    */
                    fDefImpl.markActionDirty();
                    fDefImpl.pushDeformAction();
                    incActionPopGroup();
                    fDefImpl.getCleanDeformAction();
                    paramValueChanged();
                }
                break;
            case SubToolMode.Deform:
                if (acquired) {
                    fVertImpl.vertexColor = vec4(0, 1, 1, 1);
                    fVertImpl.edgeColor   = vec4(0, 1, 1, 1);
                    fDefImpl.vertexColor = vec4(0, 0.5, 0, 1);
                    fDefImpl.edgeColor   = vec4(0, 0.5, 0, 0.5);
                    fDefImpl.pushDeformAction();
                    fVertImpl.getCleanDeformAction();
                    paramValueChanged();
                    mode = SubToolMode.Vertex;
                }
                break;
            default:
            }
            return true;
        }

        prevMode = mode;

        switch (mode) {
        case SubToolMode.Select:
            return super.update(io, impl, action, changed);
        case SubToolMode.Vertex:
            if (acquired) {
                bool result = fVertImpl.getTool().update(io, fVertImpl, vertActionId, changed);
                return result;
            }
            break;
        case SubToolMode.Deform:
            if (acquired) {
                bool result = fDefImpl.getTool().update(io, fDefImpl, defActionId, changed);
                auto parameter = incArmedParameter();
                auto deform = cast(DeformationParameterBinding)parameter.getOrAddBinding(cast(T)filter, "deform");
                deform.update(parameter.findClosestKeypoint(), fDefImpl.getOffsets());
                if (result) {
                    impl.markActionDirty();
                }
                return result;
            }
            break;
        default:
        }
        return false;
    }

    override
    void draw(Camera camera, IncMeshEditorOne impl) {
        if (mode == SubToolMode.Select)
            super.draw(camera, impl);

        if (acquired) {
            if (mode == SubToolMode.Vertex) {
                fDefImpl.draw(camera);
                fDefImpl.getTool().draw(camera, fDefImpl);
                fVertImpl.draw(camera);
                fVertImpl.getTool().draw(camera, fVertImpl);
            } else {
                fVertImpl.draw(camera);
                fVertImpl.getTool().draw(camera, fVertImpl);
                fDefImpl.draw(camera);
                fDefImpl.getTool().draw(camera, fDefImpl);
            }
        }

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

IncMeshEditorOne ngGetArmedParameterEditorFor(Node node) {
    if (auto f = cast(NodeFilter)node) {
        if (f == filter) {
            return fDefImpl;
        }
    }
    return null;
}