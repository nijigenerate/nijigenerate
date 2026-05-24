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
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijigenerate.core.dbg;
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
    // Map active NodeFilter -> its deform editor to resolve editors during Undo/Redo reliably
    IncMeshEditorOne[NodeFilter] gFilterEditors;

    final class OneTimeDeformScope {
        NodeFilter filter = null;
        IncMeshEditorOne vertImpl = null;
        IncMeshEditorOne defImpl = null;
        int vertActionId = NodeSelect.SelectActionID.None;
        int defActionId  = NodeSelect.SelectActionID.None;

    private:
        Node detachedFilterParent = null;
        ulong detachedFilterOffset = 0;
        ActionStackScope actionScope = null;
        bool explicitScopeClose = false;

    public:
        Node filterNode() {
            return cast(Node)filter;
        }

        bool active() {
            return filter !is null;
        }

        void begin() {
            if (actionScope is null)
                actionScope = ngOpenActionStackScope(ActionStackScopeUnit.OneTimeDeform);
        }

        void closeActionScope() {
            if (actionScope) {
                explicitScopeClose = true;
                scope(exit) explicitScopeClose = false;
                auto closing = actionScope;
                actionScope = null;
                closing.close();
            }
        }

        void onActionScopeClosed() {
            if (explicitScopeClose) return;
            actionScope = null;
            cleanupTarget(null, true);
            if (filter is null)
                activeScope = null;
        }

        void forceFinalize(Node target) {
            if (filter) {
                closeActionScope();
                // Remove mapping first to avoid stale lookups
                if (filter in gFilterEditors)
                    gFilterEditors.remove(filter);
                foreach (child; filterNode().children.dup) {
                    filter.applyDeformToChildren([incArmedParameter()], false);
                    filter.releaseTarget(child);
                }
                if (vertImpl)
                    vertImpl.removeFilterTarget(target);
                if (defImpl)
                    defImpl.removeFilterTarget(target);
                if (filterNode().children.length == 0) {
                    filterNode().reparent(null, 0);
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
                vertImpl = new IncMeshEditorOneFor!(T, EditMode.VertexEdit);
                vertImpl.vertexColor = vec4(0, 1, 1, 1);
                vertImpl.edgeColor   = vec4(0, 1, 1, 1);
                vertImpl.setTarget(cast(T)filter);
                vertImpl.addFilterTarget(target);
                vertActionId = NodeSelect.SelectActionID.None;

                defImpl  = new IncMeshEditorOneFor!(T, EditMode.ModelEdit);
                defImpl.vertexColor = vec4(0, 1, 0, 1);
                defImpl.edgeColor   = vec4(0, 1, 0, 1);
                defImpl.setTarget(cast(T)filter);
                defImpl.addFilterTarget(target);
                defActionId = NodeSelect.SelectActionID.None;
                setup!(T);
                // Register mapping for this filter
                gFilterEditors[filter] = defImpl;
                return true;
            }
            return false;
        }

        void setup(T: MeshGroup)() {
            vertImpl.setToolMode(VertexToolMode.Grid);
            defImpl.setToolMode(VertexToolMode.Points);
        }

        void setup(T: GridDeformer)() {
            vertImpl.setToolMode(VertexToolMode.Grid);
            defImpl.setToolMode(VertexToolMode.Grid);
        }

        void setup(T: PathDeformer)() {
            vertImpl.setToolMode(VertexToolMode.BezierDeform);
            defImpl.setToolMode(VertexToolMode.BezierDeform);
        }

        void detachForSave(Puppet puppet) {
            auto node = filterNode();
            if (node is null || node.parent is null || node.puppet !is puppet)
                return;

            detachedFilterParent = node.parent;
            auto offset = detachedFilterParent.children.countUntil(node);
            detachedFilterOffset = offset >= 0 ? cast(ulong)offset : Node.OFFSET_END;
            node.reparent(null, 0, true);
        }

        void restoreAfterSave(Puppet puppet) {
            auto node = filterNode();
            if (node is null || detachedFilterParent is null)
                return;

            node.reparent(detachedFilterParent, detachedFilterOffset, true);
            detachedFilterParent = null;
            detachedFilterOffset = 0;
        }

        void cleanupTarget(IncMeshEditorOne impl, bool allTargets) {
            if (filter is null) return;

            auto node = filterNode();
            if (allTargets) {
                foreach (child; node.children.dup) {
                    filter.releaseTarget(child);
                }
            } else if (impl && impl.getTarget() !is null) {
                if (node.children.countUntil(impl.getTarget()) >= 0)
                    filter.releaseTarget(impl.getTarget());
            }

            if (vertImpl && impl)
                vertImpl.removeFilterTarget(impl.getTarget());
            if (defImpl && impl)
                defImpl.removeFilterTarget(impl.getTarget());

            if (node.children.length == 0) {
                if (filter in gFilterEditors)
                    gFilterEditors.remove(filter);
                node.reparent(null, 0);
                filter = null;
                closeActionScope();
            }
        }

        void leaveSubTool(SubToolMode current) {
            if (!active()) return;
            final switch (current) {
            case SubToolMode.Select:
                break;
            case SubToolMode.Vertex:
                if (vertImpl) {
                    vertImpl.pushDeformAction();
                    vertImpl.applyToTarget();
                    auto parameter = incArmedParameter();
                    if (parameter) parameter.update();
                }
                break;
            case SubToolMode.Deform:
                if (defImpl)
                    defImpl.pushDeformAction();
                break;
            }
        }

        void enterSubTool(T)(SubToolMode next) {
            if (!active()) return;
            final switch (next) {
            case SubToolMode.Select:
                break;
            case SubToolMode.Vertex:
                vertImpl.vertexColor = vec4(0, 1, 1, 1);
                vertImpl.edgeColor   = vec4(0, 1, 1, 1);
                defImpl.vertexColor = vec4(0, 0.5, 0, 1);
                defImpl.edgeColor   = vec4(0, 0.5, 0, 0.5);
                vertImpl.setToolMode(vertImpl.getToolMode());
                vertImpl.getCleanDeformAction();
                break;
            case SubToolMode.Deform:
                vertImpl.vertexColor = vec4(0, 0.5, 0.5, 1);
                vertImpl.edgeColor   = vec4(0, 0.5, 0.5, 0.5);
                defImpl.vertexColor = vec4(0, 1, 0, 1);
                defImpl.edgeColor   = vec4(0, 1, 0, 1);
                defImpl.setToolMode(defImpl.getToolMode());
                defImpl.setTarget(cast(T)filter);
                defImpl.getCleanDeformAction();
                seedViewFromBinding();
                break;
            }
        }

        void seedViewFromBinding() {
            auto parameter = incArmedParameter();
            if (!parameter || defImpl is null) return;
            if (auto deformable = cast(Deformable)defImpl.getTarget()) {
                auto deform = cast(DeformationParameterBinding)parameter.getBinding(deformable, "deform");
                if (deform is null) return;
                auto kp = parameter.findClosestKeypoint();
                auto binding = deform.getValue(kp);
                Vec2Array offs = binding.vertexOffsets.dup;
                size_t targetLen = defImpl.getOffsets().length;
                if (offs.length != targetLen) {
                    Vec2Array resized;
                    resized.length = targetLen;
                    size_t n = offs.length < targetLen ? offs.length : targetLen;
                    foreach (i; 0..n) resized[i] = offs[i];
                    foreach (i; n..targetLen) resized[i] = vec2(0);
                    offs = resized;
                }
                defImpl.applyOffsets(offs);
            }
        }
    }

    OneTimeDeformScope activeScope = null;

    OneTimeDeformScope ngOneTimeDeformScope() {
        if (activeScope is null)
            activeScope = new OneTimeDeformScope();
        return activeScope;
    }

    OneTimeDeformScope ngActiveOneTimeDeformScope() {
        return activeScope;
    }

    void ngDetachOneTimeDeformFilterForSave(Puppet puppet) {
        if (activeScope)
            activeScope.detachForSave(puppet);
    }

    void ngRestoreOneTimeDeformFilterAfterSave(Puppet puppet) {
        if (activeScope)
            activeScope.restoreAfterSave(puppet);
    }

    void ngOnOneTimeDeformScopeClosed(ActionStackScopeUnit unit) {
        if (activeScope)
            activeScope.onActionScopeClosed();
    }
}

shared static this() {
    incRegisterSaveFunc(&ngDetachOneTimeDeformFilterForSave);
    ngRegisterPostSaveFunc(&ngRestoreOneTimeDeformFilterAfterSave);
    ngRegisterActionStackScopeCloseHandler(ActionStackScopeUnit.OneTimeDeform, &ngOnOneTimeDeformScopeClosed);
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
private:
    void switchSubTool(SubToolMode next) {
        if (mode == next) return;
        auto session = ngOneTimeDeformScope();
        session.leaveSubTool(mode);
        mode = next;
        session.enterSubTool!T(next);
        appliedMode = next;
    }

public:
    override
    bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
        switch (mode) {
        case SubToolMode.Select:
            return super.onDragStart(mousePos, impl);
        case SubToolMode.Vertex:
            if (acquired)
                if (auto draggable = cast(Draggable)ngOneTimeDeformScope().vertImpl.getTool())
                    return draggable.onDragStart(mousePos, ngOneTimeDeformScope().vertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired)
                if (auto draggable = cast(Draggable)ngOneTimeDeformScope().defImpl.getTool())
                    return draggable.onDragStart(mousePos, ngOneTimeDeformScope().defImpl);
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
                if (auto draggable = cast(Draggable)ngOneTimeDeformScope().vertImpl.getTool())
                    return draggable.onDragUpdate(mousePos, ngOneTimeDeformScope().vertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired)
                if (auto draggable = cast(Draggable)ngOneTimeDeformScope().defImpl.getTool())
                    return draggable.onDragUpdate(mousePos, ngOneTimeDeformScope().defImpl);
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
                if (auto draggable = cast(Draggable)ngOneTimeDeformScope().vertImpl.getTool())
                    return draggable.onDragEnd(mousePos, ngOneTimeDeformScope().vertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired)
                if (auto draggable = cast(Draggable)ngOneTimeDeformScope().defImpl.getTool())
                    return draggable.onDragEnd(mousePos, ngOneTimeDeformScope().defImpl);
            break;
        default:
            break;
        }
        return false;
    }

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        super.setToolMode(toolMode, impl);
        auto session = ngOneTimeDeformScope();
        session.initialize!T(impl.getTarget());
        acquired = session.active();
        if ((cast(T)session.filter).children.countUntil(impl.getTarget()) < 0) {
            session.filter.captureTarget(impl.getTarget());
        }
        impl.getDeformAction();
        session.begin();
        mode = SubToolMode.Vertex;
        appliedMode = mode;
    }

    override
    void finalizeToolMode(IncMeshEditorOne impl) {
        if (acquired) {
            auto session = ngOneTimeDeformScope();
            session.leaveSubTool(mode);
            session.closeActionScope();

            // Persist filter deformation into children as an undoable parameter action,
            // then release filter targets and clean up.
            auto param = incArmedParameter();
            if (session.filter && param) {
                auto kp = param.findClosestKeypoint();
                ParameterBinding[] bindings;
                foreach (child; session.filterNode().children) {
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
                    session.filter.applyDeformToChildren([param], false);
                    persistAction.updateNewState();
                    incActionPush(persistAction);
                } else {
                    // No bindings to persist; still apply to ensure state consistency
                    session.filter.applyDeformToChildren([param], false);
                }

                session.cleanupTarget(impl, true);
                incViewportNodeDeformNotifyParamValueChanged();
            }
            impl.pushDeformAction();
        }
    }

    override
    void abortToolMode(IncMeshEditorOne impl) {
        auto session = ngActiveOneTimeDeformScope();
        if (session)
            session.cleanupTarget(impl, acquired);
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
                ngOneTimeDeformScope().vertActionId = ngOneTimeDeformScope().vertImpl.getTool().peek(io, ngOneTimeDeformScope().vertImpl);
            break;
        case SubToolMode.Deform:
            if (acquired) {
                version(assert) {
                    assert(ngOneTimeDeformScope().defImpl !is null, "Deform editor missing on peek");
                }
                ngOneTimeDeformScope().defActionId = ngOneTimeDeformScope().defImpl.getTool().peek(io, ngOneTimeDeformScope().defImpl);
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

        // Apply side-effects if mode changed externally (e.g., via Undo/Redo of mode action)
        if (appliedMode != mode) {
            ngOneTimeDeformScope().enterSubTool!T(mode);
            appliedMode = mode;
        }

        if (action == OneTimeDeformActionID.SwitchMode) {
            // Group persistence + mode change into one undo step
            incActionPushGroup();
            // Capture per-target old/new modes to keep UI mode flips in history.
            auto session = ngOneTimeDeformScope();
            Node[] targets = session.defImpl ? session.defImpl.getFilterTargets() : (session.vertImpl ? session.vertImpl.getFilterTargets() : null);
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

            final switch (mode) {
            case SubToolMode.Select:
                switchSubTool(SubToolMode.Vertex);
                break;
            case SubToolMode.Vertex:
                switchSubTool(SubToolMode.Deform);
                break;
            case SubToolMode.Deform:
                switchSubTool(SubToolMode.Vertex);
                break;
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
                auto session = ngOneTimeDeformScope();
                bool result = session.vertImpl.getTool().update(io, session.vertImpl, session.vertActionId, changed);
                // When vertex count or map updates, transfer deformation to the current shape accurately
                if (session.vertImpl.vertexMapDirty) {
                    session.vertImpl.applyToTarget();
                    auto parameter = incArmedParameter();
                    if (parameter) parameter.update();
                    session.vertImpl.vertexMapDirty = false;
                }
                return result;
            }
            break;
        case SubToolMode.Deform:
            if (acquired) {
                auto session = ngOneTimeDeformScope();
                bool result = session.defImpl.getTool().update(io, session.defImpl, session.defActionId, changed);
                // 1-frame delayed write to binding: apply last frame's offsets now
                if (result) {
                    auto parameter = incArmedParameter();
                    if (parameter) {
                        auto deform = cast(DeformationParameterBinding)parameter.getOrAddBinding(cast(T)session.filter, "deform");
                        if (deform !is null)
                            deform.update(parameter.findClosestKeypoint(), session.defImpl.getOffsets());
                    }
                    session.defImpl.markActionDirty();
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
            auto session = ngOneTimeDeformScope();
            if (mode == SubToolMode.Vertex) {
//                session.defImpl.draw(camera);
//                session.defImpl.getTool().draw(camera, session.defImpl);
                session.vertImpl.draw(camera);
                session.vertImpl.getTool().draw(camera, session.vertImpl);
            } else {
                session.vertImpl.draw(camera);
                session.vertImpl.getTool().draw(camera, session.vertImpl);
                session.defImpl.draw(camera);
                session.defImpl.getTool().draw(camera, session.defImpl);
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
    // Map original deformable target to the current deform editor when filtered
    auto session = ngActiveOneTimeDeformScope();
    if (session !is null && session.filter !is null) {
        foreach (child; session.filterNode().children) {
            if (child is node) {
                if (session.filter in gFilterEditors)
                    return gFilterEditors[session.filter];
                else
                    return session.defImpl;
            }
        }
    }
    return null;
}

// Expose current active NodeFilter for OneTimeDeform to allow actions to
// resolve stale filter targets to the current instance during Undo/Redo.
NodeFilter ngCurrentNodeFilter() {
    auto session = ngActiveOneTimeDeformScope();
    return session ? session.filter : null;
}
