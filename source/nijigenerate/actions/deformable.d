module nijigenerate.actions.deformable;

import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
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
        return result;
    }

    void applyState(ref DeformableState st) {
        self.rebuffer(Vec2Array(st.vertices));
        self.clearCache();
    }
}

class GridDeformerChangeAction : GroupAction, LazyBoundAction {
public:
    struct GridState {
        float[] axisX;
        float[] axisY;
        Vec2Array deformation;
        float[] depths;
        bool hasDepths;
    }

    GridDeformer self;
    string name;

    GridState state;
    bool undoable;

    this(string name, GridDeformer self) {
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

private:
    GridState captureState() {
        GridState result;
        result.axisX = self.gridAxisX;
        result.axisY = self.gridAxisY;
        result.deformation = self.deformation.dup;
        if (auto depthMapped = cast(DepthMappedNode)self) {
            if (auto depths = depthMapped.copyDepths()) {
                result.depths = depths;
                result.hasDepths = true;
            }
        }
        return result;
    }

    void applyState(ref GridState st) {
        self.replaceGridAxes(st.axisX, st.axisY);
        self.deformation = st.deformation.dup;
        if (auto depthMapped = cast(DepthMappedNode)self) {
            depthMapped.replaceDepths(st.hasDepths ? st.depths : null);
        }
        self.clearCache();
        self.notifyChange(self, NotifyReason.StructureChanged);
    }
}
