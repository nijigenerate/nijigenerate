module nijigenerate.viewport.common.mesheditor.tools.onetimedeform;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.tools.grid : GridTool;
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
import std.algorithm;
import std.array;
import std.math : isFinite;

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
        SubToolMode currentMode = SubToolMode.Vertex;
        bool switchModeConsumed = false;
        int cachedPeekFrame = -1;
        SubToolMode cachedPeekMode = SubToolMode.Select;
        int cachedPeekAction = NodeSelect.SelectActionID.None;
        int cachedUpdateFrame = -1;
        SubToolMode cachedUpdateMode = SubToolMode.Select;
        int cachedUpdateAction = NodeSelect.SelectActionID.None;
        bool cachedUpdateResult = false;
        bool cachedUpdateChanged = false;

    private:
        struct SaveBindingBackup {
            DeformationParameterBinding binding;
            bool bindingAdded;
            Deformation[][] values;
            bool[][] isSet;
        }

        struct SaveTargetFit {
            Deformable target;
            DeformationParameterBinding binding;
            Vec2Array desired;
            Vec2Array initial;
        }

        Node detachedFilterParent = null;
        ulong detachedFilterOffset = 0;
        SaveBindingBackup[] saveBackups;
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
                currentMode = SubToolMode.Vertex;
                invalidatePeekCache();
                // Register mapping for this filter
                gFilterEditors[filter] = defImpl;
                return true;
            }
            return false;
        }

        void setup(T: GridDeformer)() {
            vertImpl.setToolMode(VertexToolMode.Grid);
            if (auto gridTool = cast(GridTool)vertImpl.getTool())
                gridTool.resetVirtualMeshAsEmpty(vertImpl);
            defImpl.setToolMode(VertexToolMode.Points);
        }

        void setup(T: PathDeformer)() {
            vertImpl.setToolMode(VertexToolMode.BezierDeform);
            defImpl.setToolMode(VertexToolMode.BezierDeform);
        }

        Deformation[][] copyValues(Deformation[][] source) {
            Deformation[][] result;
            result.length = source.length;
            foreach (x; 0 .. source.length) {
                result[x].length = source[x].length;
                foreach (y; 0 .. source[x].length)
                    result[x][y] = source[x][y];
            }
            return result;
        }

        bool[][] copyIsSet(bool[][] source) {
            bool[][] result;
            result.length = source.length;
            foreach (x; 0 .. source.length)
                result[x] = source[x].dup;
            return result;
        }

        DeformationParameterBinding backupTargetBinding(Parameter param, Deformable target) {
            auto existing = cast(DeformationParameterBinding)param.getBinding(target, "deform");
            bool bindingAdded = existing is null;
            auto binding = cast(DeformationParameterBinding)param.getOrAddBinding(target, "deform");
            if (binding is null)
                return null;

            saveBackups ~= SaveBindingBackup(
                binding,
                bindingAdded,
                bindingAdded ? null : copyValues(binding.values),
                bindingAdded ? null : copyIsSet(binding.isSet_)
            );
            return binding;
        }

        void detachFilterNodeForSave(Puppet puppet) {
            auto node = filterNode();
            if (node is null || node.parent is null || node.puppet !is puppet)
                return;

            detachedFilterParent = node.parent;
            auto offset = detachedFilterParent.children.countUntil(node);
            detachedFilterOffset = offset >= 0 ? cast(ulong)offset : Node.OFFSET_END;
            node.reparent(null, 0, true);
        }

        void flushCurrentSubToolForSave() {
            if (!active())
                return;

            auto parameter = incArmedParameter();
            final switch (currentMode) {
            case SubToolMode.Select:
                break;
            case SubToolMode.Vertex:
                if (vertImpl) {
                    applyVertexToolToFilter();
                }
                break;
            case SubToolMode.Deform:
                if (defImpl && parameter) {
                    auto deform = cast(DeformationParameterBinding)parameter.getOrAddBinding(cast(Node)filter, "deform");
                    if (deform !is null) {
                        auto offsets = defImpl.getOffsets();
                        normalizeDeformationBinding(deform, offsets.length);
                        deform.update(parameter.findClosestKeypoint(), offsets);
                        parameter.update();
                        incActivePuppet().update();
                    }
                }
                break;
            }
        }

        bool applyVertexToolToFilter() {
            if (!vertImpl)
                return false;

            bool applied = false;
            if (auto gridTool = cast(GridTool)vertImpl.getTool()) {
                applied = gridTool.applyVirtualMeshToTarget(vertImpl);
                if (!applied)
                    return false;
            } else {
                vertImpl.applyToTarget();
                applied = true;
            }

            if (auto parameter = incArmedParameter())
                parameter.update();
            incActivePuppet().update();
            vertImpl.vertexMapDirty = false;
            return applied;
        }

        void detachForSave(Puppet puppet) {
            auto node = filterNode();
            if (node is null || node.puppet !is puppet)
                return;

            flushCurrentSubToolForSave();

            auto param = incArmedParameter();
            if (param is null) {
                detachFilterNodeForSave(puppet);
                return;
            }

            SaveTargetFit[] fits;
            auto kp = param.findClosestKeypoint();
            foreach (child; node.children) {
                auto deformable = cast(Deformable)child;
                if (deformable is null)
                    continue;
                auto binding = backupTargetBinding(param, deformable);
                if (binding is null)
                    continue;
                Vec2Array initial = binding.getValue(kp).vertexOffsets.dup;
                initial.length = deformable.vertices.length;
                Vec2Array desired = deformable.deformation.dup;
                desired.length = deformable.vertices.length;
                fits ~= SaveTargetFit(deformable, binding, desired, initial);
            }

            detachFilterNodeForSave(puppet);
            param.update();
            incActivePuppet().update();

            foreach (fit; fits)
                fitOffsetsForFinalResult(param, fit.binding, kp, fit.target, fit.initial, fit.desired);
        }

        void restoreAfterSave(Puppet puppet) {
            foreach (backup; saveBackups) {
                if (backup.binding is null)
                    continue;
                auto param = backup.binding.parameter;
                if (backup.bindingAdded) {
                    if (param)
                        param.removeBinding(backup.binding);
                } else {
                    backup.binding.values = backup.values;
                    backup.binding.isSet_ = backup.isSet;
                    backup.binding.reInterpolate();
                }
            }
            saveBackups.length = 0;

            auto node = filterNode();
            if (node is null || detachedFilterParent is null)
                return;

            node.reparent(detachedFilterParent, detachedFilterOffset, true);
            detachedFilterParent = null;
            detachedFilterOffset = 0;

            if (auto param = incArmedParameter())
                param.update();
            incActivePuppet().update();
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
                switchModeConsumed = false;
                invalidatePeekCache();
                closeActionScope();
            }
        }

        void invalidatePeekCache() {
            cachedPeekFrame = -1;
            cachedPeekAction = NodeSelect.SelectActionID.None;
            cachedUpdateFrame = -1;
            cachedUpdateAction = NodeSelect.SelectActionID.None;
            cachedUpdateResult = false;
            cachedUpdateChanged = false;
        }

        void resetCurrentTool(IncMeshEditorOne impl) {
            if (impl is null || impl.getTool() is null)
                return;
            impl.getTool().setToolMode(impl.getToolMode(), impl);
        }

        void leaveSubTool(SubToolMode current) {
            if (!active()) return;
            final switch (current) {
            case SubToolMode.Select:
                break;
            case SubToolMode.Vertex:
                if (vertImpl) {
                    vertImpl.pushDeformAction();
                    applyVertexToolToFilter();
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
            currentMode = next;
            invalidatePeekCache();
            final switch (next) {
            case SubToolMode.Select:
                break;
            case SubToolMode.Vertex:
                vertImpl.vertexColor = vec4(0, 1, 1, 1);
                vertImpl.edgeColor   = vec4(0, 1, 1, 1);
                defImpl.vertexColor = vec4(0, 0.6, 0, 1);
                defImpl.edgeColor   = vec4(0, 0.6, 0, 1);
                resetCurrentTool(vertImpl);
                vertImpl.getCleanDeformAction();
                break;
            case SubToolMode.Deform:
                vertImpl.vertexColor = vec4(0, 0.6, 0.6, 1);
                vertImpl.edgeColor   = vec4(0, 0.6, 0.6, 1);
                defImpl.vertexColor = vec4(0, 1, 0, 1);
                defImpl.edgeColor   = vec4(0, 1, 0, 1);
                resetCurrentTool(defImpl);
                defImpl.setTarget(cast(T)filter);
                defImpl.getCleanDeformAction();
                seedViewFromBinding();
                break;
            }
        }

        void ensureSubToolReady(T)(SubToolMode next) {
            if (!active()) return;
            if (currentMode != next) {
                enterSubTool!T(next);
                return;
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

        void normalizeDeformationBinding(DeformationParameterBinding binding, size_t length) {
            if (binding is null)
                return;
            foreach (x; 0 .. binding.values.length) {
                foreach (y; 0 .. binding.values[x].length) {
                    auto offsets = binding.values[x][y].vertexOffsets.dup;
                    if (offsets.length == length)
                        continue;
                    Vec2Array resized;
                    resized.length = length;
                    auto count = min(offsets.length, length);
                    foreach (i; 0 .. count)
                        resized[i] = offsets[i];
                    foreach (i; count .. length)
                        resized[i] = vec2(0);
                    binding.values[x][y].vertexOffsets = resized;
                }
            }
        }

        float squaredError(Vec2Array lhs, Vec2Array rhs) {
            auto count = min(lhs.length, rhs.length);
            if (count == 0) return float.max;
            double total = 0;
            foreach (i; 0 .. count) {
                auto d = lhs[i] - rhs[i];
                if (!isFinite(d.x) || !isFinite(d.y))
                    return float.max;
                total += cast(double)d.x * d.x + cast(double)d.y * d.y;
            }
            return cast(float)(total / count);
        }

        Vec2Array evaluateWithCandidate(Parameter param, DeformationParameterBinding binding, vec2u keypoint, Vec2Array candidate) {
            normalizeDeformationBinding(binding, candidate.length);
            binding.update(keypoint, candidate);
            param.update();
            incActivePuppet().update();

            auto target = cast(Deformable)binding.targetNode;
            return target ? target.deformation.dup : Vec2Array.init;
        }

        Vec2Array fitOffsetsForFinalResult(Parameter param, DeformationParameterBinding binding, vec2u keypoint, Deformable target, Vec2Array initial, Vec2Array desired) {
            if (desired.length != target.vertices.length)
                return initial;

            Vec2Array candidate = initial.dup;
            candidate.length = desired.length;

            auto evaluated = evaluateWithCandidate(param, binding, keypoint, candidate);
            float currentError = squaredError(evaluated, desired);
            Vec2Array best = candidate.dup;
            float bestError = currentError;
            float step = 1.0f;

            enum MaxIterations = 10;
            enum MinStep = 0.015625f;
            enum Epsilon = 0.0001f;

            foreach (_; 0 .. MaxIterations) {
                if (bestError <= Epsilon)
                    break;

                Vec2Array trial = candidate.dup;
                auto count = min(trial.length, desired.length, evaluated.length);
                foreach (i; 0 .. count) {
                    trial[i] += (desired[i] - evaluated[i]) * step;
                }

                auto trialEvaluated = evaluateWithCandidate(param, binding, keypoint, trial);
                float trialError = squaredError(trialEvaluated, desired);
                if (trialError < currentError) {
                    candidate = trial;
                    evaluated = trialEvaluated;
                    currentError = trialError;
                    if (trialError < bestError) {
                        best = trial.dup;
                        bestError = trialError;
                    }
                    step = min(step * 1.25f, 1.0f);
                } else {
                    step *= 0.5f;
                    if (step < MinStep)
                        break;
                }
            }

            binding.update(keypoint, best);
            param.update();
            incActivePuppet().update();
            return best;
        }

        void applyToTargetsWithFitting(Parameter param, vec2u keypoint) {
            struct TargetFit {
                Deformable target;
                DeformationParameterBinding binding;
                Vec2Array desired;
                Vec2Array initial;
            }

            TargetFit[] fits;
            foreach (child; filterNode().children) {
                auto deformable = cast(Deformable)child;
                if (deformable is null)
                    continue;
                auto binding = cast(DeformationParameterBinding)param.getOrAddBinding(deformable, "deform");
                if (binding is null)
                    continue;
                Vec2Array initial = binding.getValue(keypoint).vertexOffsets.dup;
                initial.length = deformable.vertices.length;
                Vec2Array desired = deformable.deformation.dup;
                desired.length = deformable.vertices.length;
                fits ~= TargetFit(deformable, binding, desired, initial);
            }

            foreach (child; filterNode().children.dup)
                filter.releaseTarget(child);

            if (auto filterBinding = param.getBinding(cast(Node)filter, "deform"))
                param.removeBinding(filterBinding);

            foreach (fit; fits)
                fitOffsetsForFinalResult(param, fit.binding, keypoint, fit.target, fit.initial, fit.desired);
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

    bool registeredCallbacks = false;

    void ngEnsureOneTimeDeformCallbacks() {
        if (registeredCallbacks)
            return;
        registeredCallbacks = true;
        incRegisterSaveFunc(&ngDetachOneTimeDeformFilterForSave);
        ngRegisterPostSaveFunc(&ngRestoreOneTimeDeformFilterAfterSave);
        ngRegisterActionStackScopeCloseHandler(ActionStackScopeUnit.OneTimeDeform, &ngOnOneTimeDeformScopeClosed);
    }
}

class OneTimeDeformBase :  NodeSelect {
    SubToolMode mode;
    SubToolMode prevMode = SubToolMode.Vertex;
    SubToolMode appliedMode = SubToolMode.Vertex; // last applied UI/impl state
    bool acquired = false;

    enum OneTimeDeformActionID {
        SwitchMode = 10_000,
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
        ngEnsureOneTimeDeformCallbacks();
        auto session = ngOneTimeDeformScope();
        session.initialize!T(impl.getTarget());
        acquired = session.active();
        if (session.vertImpl && !session.vertImpl.getFilterTargets().canFind(impl.getTarget()))
            session.vertImpl.addFilterTarget(impl.getTarget());
        if (session.defImpl && !session.defImpl.getFilterTargets().canFind(impl.getTarget()))
            session.defImpl.addFilterTarget(impl.getTarget());
        if ((cast(T)session.filter).children.countUntil(impl.getTarget()) < 0) {
            session.filter.captureTarget(impl.getTarget());
            // captureTarget changes the filter mesh. The inner editors were
            // created before capture, so refresh them before any subtool uses
            // hit-testing or offsets against the filter.
            if (session.defImpl) {
                session.defImpl.setTarget(cast(T)session.filter);
                session.defImpl.setToolMode(session.defImpl.getToolMode());
            }
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

                    // Fit the baked target offsets so the existing downstream
                    // deformers reproduce the final one-time edit result.
                    session.applyToTargetsWithFitting(param, kp);
                    persistAction.updateNewState();
                    incActionPush(persistAction);
                } else {
                    foreach (child; session.filterNode().children.dup)
                        session.filter.releaseTarget(child);
                    if (auto filterBinding = param.getBinding(cast(Node)session.filter, "deform"))
                        param.removeBinding(filterBinding);
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

        int peekSharedSubTool(OneTimeDeformScope session, SubToolMode subMode, int delegate() evaluate) {
            auto frame = igGetFrameCount();
            if (session.cachedPeekFrame == frame && session.cachedPeekMode == subMode)
                return session.cachedPeekAction;
            auto action = evaluate();
            session.cachedPeekFrame = frame;
            session.cachedPeekMode = subMode;
            session.cachedPeekAction = action;
            return action;
        }

        switch (mode) {
        case SubToolMode.Select:
            return super.peek(io, impl);
        case SubToolMode.Vertex:
            if (acquired) {
                auto session = ngOneTimeDeformScope();
                return session.vertActionId = peekSharedSubTool(session, mode, () {
                    return session.vertImpl.getTool().peek(io, session.vertImpl);
                });
            }
            break;
        case SubToolMode.Deform:
            if (acquired) {
                auto session = ngOneTimeDeformScope();
                version(assert) {
                    assert(session.defImpl !is null, "Deform editor missing on peek");
                }
                return session.defActionId = peekSharedSubTool(session, mode, () {
                    return session.defImpl.getTool().peek(io, session.defImpl);
                });
            }
            break;
        default:
        }

        return SelectActionID.None;
    }

    override
    int unify(int[] actions) {
        foreach (a; actions) {
            if (a == OneTimeDeformActionID.SwitchMode)
                return OneTimeDeformActionID.SwitchMode;
        }

        auto session = ngActiveOneTimeDeformScope();
        final switch (mode) {
        case SubToolMode.Select:
            return super.unify(actions);
        case SubToolMode.Vertex:
            if (acquired && session && session.vertImpl)
                return session.vertImpl.getTool().unify(actions);
            break;
        case SubToolMode.Deform:
            if (acquired && session && session.defImpl)
                return session.defImpl.getTool().unify(actions);
            break;
        }
        return SelectActionID.None;
    }

    override bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        incStatusTooltip(_("Switch Mode"), _("TAB"));

        bool updateSharedSubTool(OneTimeDeformScope session, SubToolMode subMode, int subAction, bool delegate(out bool) evaluate, out bool subChanged) {
            auto frame = igGetFrameCount();
            if (session.cachedUpdateFrame == frame &&
                session.cachedUpdateMode == subMode &&
                session.cachedUpdateAction == subAction) {
                subChanged = session.cachedUpdateChanged;
                return session.cachedUpdateResult;
            }

            bool localChanged = false;
            auto result = evaluate(localChanged);
            session.cachedUpdateFrame = frame;
            session.cachedUpdateMode = subMode;
            session.cachedUpdateAction = subAction;
            session.cachedUpdateResult = result;
            session.cachedUpdateChanged = localChanged;
            subChanged = localChanged;
            return result;
        }

        // Apply side-effects if mode changed externally (e.g., via Undo/Redo of mode action)
        if (appliedMode != mode) {
            ngOneTimeDeformScope().enterSubTool!T(mode);
            appliedMode = mode;
        }
        auto readySession = ngOneTimeDeformScope();
        if (acquired)
            readySession.ensureSubToolReady!T(mode);

        auto sessionForAction = ngOneTimeDeformScope();
        if (action != OneTimeDeformActionID.SwitchMode)
            sessionForAction.switchModeConsumed = false;

        if (action == OneTimeDeformActionID.SwitchMode) {
            if (sessionForAction.switchModeConsumed)
                return false;
            sessionForAction.switchModeConsumed = true;
            // Group persistence + mode change into one undo step
            incActionPushGroup();
            // Capture per-target old/new modes to keep UI mode flips in history.
            auto session = sessionForAction;
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

            SubToolMode nextMode = mode;
            final switch (mode) {
            case SubToolMode.Select:
                nextMode = SubToolMode.Vertex;
                break;
            case SubToolMode.Vertex:
                nextMode = SubToolMode.Deform;
                break;
            case SubToolMode.Deform:
                nextMode = SubToolMode.Vertex;
                break;
            }
            switchSubTool(nextMode);
            if (targets && targets.length > 0 && newModes.length == targets.length) {
                foreach (i, t; targets) {
                    auto ed = ngGetEditorFor(t);
                    auto tool = ed ? cast(OneTimeDeformBase)ed.getTool() : null;
                    if (tool is null)
                        continue;
                    tool.mode = newModes[i];
                    tool.appliedMode = newModes[i];
                }
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
                bool result = updateSharedSubTool(session, mode, action, (out bool localChanged) {
                    return session.vertImpl.getTool().update(io, session.vertImpl, action, localChanged);
                }, changed);
                // When vertex count or map updates, transfer deformation to the current shape accurately
                if (session.vertImpl.vertexMapDirty)
                    session.applyVertexToolToFilter();
                return result;
            }
            break;
        case SubToolMode.Deform:
            if (acquired) {
                auto session = ngOneTimeDeformScope();
                bool result = updateSharedSubTool(session, mode, action, (out bool localChanged) {
                    return session.defImpl.getTool().update(io, session.defImpl, action, localChanged);
                }, changed);
                if (changed) {
                    auto parameter = incArmedParameter();
                    if (parameter) {
                        auto deform = cast(DeformationParameterBinding)parameter.getOrAddBinding(cast(T)session.filter, "deform");
                        if (deform !is null) {
                            auto offsets = session.defImpl.getOffsets();
                            session.normalizeDeformationBinding(deform, offsets.length);
                            deform.update(parameter.findClosestKeypoint(), offsets);
                            parameter.update();
                            incActivePuppet().update();
                        }
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
                session.defImpl.draw(camera);
                session.defImpl.getTool().draw(camera, session.defImpl);
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

class ToolInfoImpl(T: OneTimeDeform!GridDeformer) : ToolInfoBase!(T) {
    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (deformOnly)
            return super.viewportTools(deformOnly, toolMode, editors);
        return false;
    }
    override bool canUse(bool deformOnly, Node[] targets) { return deformOnly; }
    override VertexToolMode mode() { return VertexToolMode.AltGridDeform; };
    override string icon() { return "";}
    override string description() { return _("Grid deformation");}
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
    override string icon() { return "";}
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
