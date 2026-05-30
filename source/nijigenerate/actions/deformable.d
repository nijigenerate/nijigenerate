module nijigenerate.actions.deformable;

import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijigenerate.ext.param : ExParameterGroup;
import nijilive;
import nijilive.math : Vec2Array;
import std.format;
import i18n;

/**
    Action to add parameter to active puppet.
*/
class DeformableChangeAction : GroupAction, LazyBoundAction {
private:
    void copy(ref MeshData src, ref MeshData dst) {
        dst.vertices = src.vertices.dup;
        dst.uvs      = src.uvs.dup;
        dst.indices  = src.indices.dup;
        dst.origin   = src.origin;
    }
public:
    struct DeformableState {
        vec2[] vertices;
        float[] depths;
        bool hasDepths;
    }

    Deformable self;
    string name;

    DeformableState state;
    bool   undoable;

    this(string name, Deformable self) {
        super();
        this.name = name;
        this.self = self;
        this.undoable = true;
        state = captureState();
    }

    override
    void updateNewState() {}

    override
    void clear() {}

    void addBinding(Parameter param, ParameterBinding binding) {
        addAction(new ParameterBindingAddAction(param, binding));
    }

    /**
        Rollback
    */
    override
    void rollback() {
        if (undoable) {
            auto current = captureState();
            applyState(state);
            state = current;
            undoable = false;
        }
        super.rollback();
    }

    /**
        Redo
    */
    override
    void redo() {
        if (!undoable) {
            auto current = captureState();
            applyState(state);
            state = current;
            undoable = true;
        }
        super.redo();
    }

    /**
        Describe the action
    */
    override
    string describe() {
        return _("Changed vertices of %s").format(self.name);
    }

    /**
        Describe the action
    */
    override
    string describeUndo() {
        return _("Vertices of %s was changed").format(self.name);
    }

    /**
        Gets name of this action
    */
    override
    string getName() {
        return this.stringof;
    }
    
    override bool merge(Action other) { return false; }
    override bool canMerge(Action other) { return false; }

private:
    DeformableState captureState() {
        DeformableState result;
        result.vertices = self.vertices.toArray();
        if (auto depthMapped = cast(DepthMappedNode)self) {
            result.depths = depthMapped.copyDepths();
            result.hasDepths = true;
        }
        return result;
    }

    void applyState(ref DeformableState st) {
        self.rebuffer(Vec2Array(st.vertices));
        if (st.hasDepths) {
            if (auto depthMapped = cast(DepthMappedNode)self)
                depthMapped.replaceDepths(st.depths);
        }
        self.clearCache();
        import nijigenerate.viewport.vertex : ngRefreshDeformableCommandEditors;
        ngRefreshDeformableCommandEditors(self);
    }
}

class GridDeformerDefineAction : LazyBoundAction {
private:
    struct BindingState {
        DeformationParameterBinding binding;
        Deformation[][] values;
        bool[][] isSet;
    }

    struct State {
        vec2[] vertices;
        float[] depths;
        bool hasDepths;
        BindingState[] bindings;
    }

    Deformable self;
    string name;
    State oldState;
    State newState;

    static Deformation[][] dupValues(Deformation[][] values) {
        Deformation[][] result;
        result.length = values.length;
        foreach (x, row; values)
            result[x] = row.dup;
        return result;
    }

    static bool[][] dupIsSet(bool[][] values) {
        bool[][] result;
        result.length = values.length;
        foreach (x, row; values)
            result[x] = row.dup;
        return result;
    }

    State captureState() {
        State result;
        result.vertices = self.vertices.toArray();
        if (auto depthMapped = cast(DepthMappedNode)self) {
            result.depths = depthMapped.copyDepths();
            result.hasDepths = true;
        }

        foreach (param; incActivePuppet().parameters) {
            void captureBinding(Parameter p) {
                auto binding = cast(DeformationParameterBinding)p.getBinding(self, "deform");
                if (binding is null)
                    return;
                BindingState bindingState;
                bindingState.binding = binding;
                bindingState.values = dupValues(binding.values);
                bindingState.isSet = dupIsSet(binding.isSet_);
                result.bindings ~= bindingState;
            }

            if (auto group = cast(ExParameterGroup)param) {
                foreach (child; group.children)
                    captureBinding(child);
            } else {
                captureBinding(param);
            }
        }

        return result;
    }

    void applyState(ref State state) {
        self.rebuffer(Vec2Array(state.vertices));
        if (state.hasDepths) {
            if (auto depthMapped = cast(DepthMappedNode)self)
                depthMapped.replaceDepths(state.depths);
        }

        foreach (bindingState; state.bindings) {
            bindingState.binding.values = dupValues(bindingState.values);
            bindingState.binding.isSet_ = dupIsSet(bindingState.isSet);
            bindingState.binding.reInterpolate();
        }

        incActivePuppet().resetDrivers();
        self.clearCache();
        self.notifyChange(cast(Node)self, NotifyReason.StructureChanged);
        import nijigenerate.viewport.vertex : ngRefreshDeformableCommandEditors;
        ngRefreshDeformableCommandEditors(self);
    }

public:
    this(string name, Deformable self) {
        this.name = name;
        this.self = self;
        this.oldState = captureState();
    }

    override
    void updateNewState() {
        this.newState = captureState();
    }

    override
    void clear() {}

    override
    void rollback() {
        applyState(oldState);
    }

    override
    void redo() {
        applyState(newState);
    }

    override
    string describe() {
        return _("Changed grid of %s").format(self.name);
    }

    override
    string describeUndo() {
        return _("Grid of %s was changed").format(self.name);
    }

    override
    string getName() {
        return this.stringof;
    }

    override bool merge(Action other) { return false; }
    override bool canMerge(Action other) { return false; }
}
