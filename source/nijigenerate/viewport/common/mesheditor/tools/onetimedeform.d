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
    // Map active NodeFilter -> its deform editor to resolve editors during Undo/Redo reliably
    IncMeshEditorOne[NodeFilter] gFilterEditors;

    void forceFinalize(Node target) {
        if (filter) {
            // Remove mapping first to avoid stale lookups
            if (cast(NodeFilter)filter in gFilterEditors)
                gFilterEditors.remove(cast(NodeFilter)filter);
            foreach (child; (cast(Node)filter).children) {
                filter.applyDeformToChildren([incArmedParameter()], false);
                filter.releaseTarget(child);
            }
            if (fVertImpl)
                fVertImpl.removeFilterTarget(target);
            if (fDefImpl)
                fDefImpl.removeFilterTarget(target);
            if ((cast(Node)filter).children.length == 0) {
                (cast(Node)filter).reparent(null, 0);
                filter = null;
            }
            incViewportNodeDeformNotifyParamValueChanged();
        }        
    }

    bool initialize(T)(Node target, Node currTarget = null) {
        if (filter is null || cast(T)filter is null) {
            if (filter)
                forceFinalize(currTarget);
            filter = new T(incActivePuppet().root);
            fVertImpl = new IncMeshEditorOneFor!(T, EditMode.VertexEdit);
            fVertImpl.vertexColor = vec4(0, 1, 1, 1);
            fVertImpl.edgeColor   = vec4(0, 1, 1, 1);
            fVertImpl.setTarget(cast(T)filter);
            fVertImpl.addFilterTarget(target);
            vertActionId = NodeSelect.SelectActionID.None;

            fDefImpl  = new IncMeshEditorOneFor!(T, EditMode.ModelEdit);
            fDefImpl.vertexColor = vec4(0, 1, 0, 1);
            fDefImpl.edgeColor   = vec4(0, 1, 0, 1);
            fDefImpl.setTarget(cast(T)filter);
            fDefImpl.addFilterTarget(target);
            defActionId = NodeSelect.SelectActionID.None;
            setup!(T);
            // Register mapping for this filter
            gFilterEditors[cast(NodeFilter)filter] = fDefImpl;
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

class OneTimeDeformBase :  NodeSelect {
    SubToolMode mode;
    SubToolMode prevMode = SubToolMode.Vertex;
    SubToolMode appliedMode = SubToolMode.Vertex; // last applied UI/impl state
    bool acquired = false;

    enum OneTimeDeformActionID {
        SwitchMode = cast(int)(SelectActionID.End),
        End
    }

}

class OneTimeDeform(T) : OneTimeDeformBase {
public:
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
        acquired = initialize!T(impl.getTarget());
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

            // Persist filter deformation into children as an undoable parameter action,
            // then release filter targets and clean up.
            auto param = incArmedParameter();
            if (filter && param) {
                auto kp = param.findClosestKeypoint();
                ParameterBinding[] bindings;
                foreach (child; (cast(Node)filter).children) {
                    if (auto deformable = cast(Deformable)child) {
                        auto b = param.getOrAddBinding(deformable, "deform");
                        if (b !is null)
                            bindings ~= b;
                    }
                }

                // Create action capturing old values first
                if (bindings.length > 0) {
                    import nijigenerate.actions.parameter : ParameterChangeBindingsValueAction;
                    auto persistAction = new ParameterChangeBindingsValueAction(_("apply deform"), param, bindings, kp.x, kp.y);

                    // Apply changes to children, then capture new values
                    filter.applyDeformToChildren([param], false);
                    persistAction.updateNewState();
                    incActionPush(persistAction);
                } else {
                    // No bindings to persist; still apply to ensure state consistency
                    filter.applyDeformToChildren([param], false);
                }

                foreach (child; (cast(Node)filter).children) {
                    filter.releaseTarget(child);
                }
                if (fVertImpl)
                    fVertImpl.removeFilterTarget(impl.getTarget());
                if (fDefImpl)
                    fDefImpl.removeFilterTarget(impl.getTarget());
                if ((cast(Node)filter).children.length == 0) {
                    (cast(Node)filter).reparent(null, 0);
                    filter = null;
                }
                incViewportNodeDeformNotifyParamValueChanged();
            }
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
            if (acquired) {
                version(assert) {
                    assert(fDefImpl !is null, "Deform editor missing on peek");
                }
                defActionId = fDefImpl.getTool().peek(io, fDefImpl);
            }
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
        // Deform表示をバインディングから読み取って反映（読み取り専用。書き込みはしない）
        void seedViewFromBinding() {
            auto parameter = incArmedParameter();
            if (!parameter) return;
            if (auto deformable = cast(Deformable)fDefImpl.getTarget()) {
                auto deform = cast(DeformationParameterBinding)parameter.getBinding(deformable, "deform");
                if (deform is null) return;
                auto kp = parameter.findClosestKeypoint();
                auto binding = deform.getValue(kp);
                vec2[] offs = binding.vertexOffsets.dup;
                size_t targetLen = fDefImpl.getOffsets().length;
                if (offs.length != targetLen) {
                    vec2[] resized;
                    resized.length = targetLen;
                    size_t n = offs.length < targetLen ? offs.length : targetLen;
                    foreach (i; 0..n) resized[i] = offs[i];
                    foreach (i; n..targetLen) resized[i] = vec2(0);
                    offs = resized;
                }
                fDefImpl.applyOffsets(offs);
            }
        }

        // Apply side-effects if mode changed externally (e.g., via Undo/Redo of mode action)
        if (appliedMode != mode) {
            final switch (mode) {
                case SubToolMode.Deform:
                    if (acquired) {
                        fVertImpl.vertexColor = vec4(0, 0.5, 0.5, 1);
                        fVertImpl.edgeColor   = vec4(0, 0.5, 0.5, 0.5);
                        fDefImpl.vertexColor = vec4(0, 1, 0, 1);
                        fDefImpl.edgeColor   = vec4(0, 1, 0, 1);
                        fDefImpl.setTarget(cast(T)filter);
                        fDefImpl.getCleanDeformAction();
                        seedViewFromBinding();
                    }
                    break;
                case SubToolMode.Vertex:
                    if (acquired) {
                        fVertImpl.vertexColor = vec4(0, 1, 1, 1);
                        fVertImpl.edgeColor   = vec4(0, 1, 1, 1);
                        fDefImpl.vertexColor = vec4(0, 0.5, 0, 1);
                        fDefImpl.edgeColor   = vec4(0, 0.5, 0, 0.5);
                        fVertImpl.getCleanDeformAction();
                    }
                    break;
                case SubToolMode.Select:
                    break;
            }
            appliedMode = mode;
        }

        if (action == OneTimeDeformActionID.SwitchMode) {
            // Group persistence + mode change into one undo step
            incActionPushGroup();
            // Capture per-target old/new modes to keep UI mode flips in history.
            Node[] targets = fDefImpl ? fDefImpl.getFilterTargets() : (fVertImpl ? fVertImpl.getFilterTargets() : null);
            SubToolMode[] oldModes;
            SubToolMode[] newModes;
            if (targets && targets.length > 0) {
                foreach (t; targets) {
                    auto ed = ngGetEditorFor(t);
                    auto tool = ed ? cast(OneTimeDeformBase)ed.getTool() : null;
                    auto m = tool ? tool.mode : mode;
                    oldModes ~= m;
                    // Toggle rule: Vertex <-> Deform, others remain.
                    final switch (m) {
                        case SubToolMode.Vertex: newModes ~= SubToolMode.Deform; break;
                        case SubToolMode.Deform: newModes ~= SubToolMode.Vertex; break;
                        case SubToolMode.Select: newModes ~= SubToolMode.Select; break;
                    }
                }
            }

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
                    fVertImpl.pushDeformAction();
                    fVertImpl.applyToTarget();
                    if (auto deformable = cast(Deformable)filter) {
                        auto parameter = incArmedParameter();
                        parameter.update();
                    }
                    fDefImpl.setTarget(cast(T)filter);
                    // Reset deform editor baseline; do not push here
                    fDefImpl.getCleanDeformAction();
                    // 画面表示を現在のバインディングからシード
                    seedViewFromBinding();
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
                    mode = SubToolMode.Vertex;
                }
                break;
            default:
            }
            // Push mode change action after switching internal state, so redo/undo reflect UI mode.
            import nijigenerate.actions.mesheditor : SubToolModeChangeAction;
            if (targets && targets.length > 0 && oldModes.length == targets.length && newModes.length == targets.length) {
                incActionPush(new SubToolModeChangeAction(targets, oldModes, newModes));
            }
            incActionPopGroup();
            return true;
        }

        prevMode = mode;

        switch (mode) {
        case SubToolMode.Select:
            return super.update(io, impl, action, changed);
        case SubToolMode.Vertex:
            if (acquired) {
                bool result = fVertImpl.getTool().update(io, fVertImpl, vertActionId, changed);
                // 頂点数やマップ更新が起きたら、変形を現在の形状へ正確に移す
                if (fVertImpl.vertexMapDirty) {
                    fVertImpl.applyToTarget();
                    auto parameter = incArmedParameter();
                    if (parameter) parameter.update();
                    fVertImpl.vertexMapDirty = false;
                }
                return result;
            }
            break;
        case SubToolMode.Deform:
            if (acquired) {
                bool result = fDefImpl.getTool().update(io, fDefImpl, defActionId, changed);
                // 1-frame delayed write to binding: apply last frame's offsets now
                if (result) {
                    auto parameter = incArmedParameter();
                    if (parameter) {
                        auto deform = cast(DeformationParameterBinding)parameter.getOrAddBinding(cast(T)filter, "deform");
                        if (deform !is null)
                            deform.update(parameter.findClosestKeypoint(), fDefImpl.getOffsets());
                    }
                    fDefImpl.markActionDirty();
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
//                fDefImpl.draw(camera);
//                fDefImpl.getTool().draw(camera, fDefImpl);
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
    override bool canUse(bool deformOnly, Node[] targets) { return deformOnly; }
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
    override bool canUse(bool deformOnly, Node[] targets) { return deformOnly; }
    override VertexToolMode mode() { return VertexToolMode.AltBezierDeform; };
    override string icon() { return "";}
    override string description() { return _("Path deformation");}
}

IncMeshEditorOne ngGetArmedParameterEditorFor(Node node) {
    if (auto f = cast(NodeFilter)node) {
        if (f in gFilterEditors)
            return gFilterEditors[f];
    }
    return null;
}

// Expose current active NodeFilter for OneTimeDeform to allow actions to
// resolve stale filter targets to the current instance during Undo/Redo.
NodeFilter ngCurrentNodeFilter() {
    return cast(NodeFilter)filter;
}
