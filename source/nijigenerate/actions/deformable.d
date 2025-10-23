module nijigenerate.actions.deformable;

import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate;
import nijilive;
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
        result.vertices = self.vertices.dup;
        return result;
    }

    void applyState(ref DeformableState st) {
        self.rebuffer(st.vertices);
        self.clearCache();
    }
}
