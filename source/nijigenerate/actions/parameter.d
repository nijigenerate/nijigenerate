/*
    Copyright © 2020-2023,2022 Inochi2D Project
    Copyright ©           2024 nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.actions.parameter;

import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.actions.binding;
import nijigenerate;
import nijilive;
import nijilive.core.nodes.drivers; // Driver
import std.format;
import i18n;
import std.algorithm.searching: countUntil;
import std.algorithm.mutation : remove;

/**
    Action to add parameter to active puppet.
*/
class ParameterAddRemoveAction(bool added = true) : Action {
public:
    Parameter self;
    Puppet puppet;
    Driver[] drivers;
//    Parameter[]* parentList;
    ExParameterGroup originalParent;
    long indexInGroup;

    this(Parameter self, Parameter[]* parentList) {
        this(self, incActivePuppet());
    }
    this(Parameter self) {
        this(self, incActivePuppet());
    }
    this(Parameter self, Puppet puppet) {
        this.self = self;
        this.puppet = puppet;
//        this.parentList = parentList;

        auto exParam = cast(ExParameter)self;
        originalParent = (exParam !is null)? exParam.getParent(): null;
        indexInGroup = -1;

        // Find drivers
        foreach(ref driver; puppet.getDrivers()) {
            if (SimplePhysics sf = cast(SimplePhysics)driver) {
                if (sf.param !is null && sf.param.uuid == self.uuid) {
                    drivers ~= driver;
                }
            }
        }

        // Empty drivers
        foreach(ref driver; drivers) {
            if (SimplePhysics sf = cast(SimplePhysics)driver) {
                sf.param = null;
            }
        }
        notifyParameterResourceChanged();
    }

    /**
        Rollback
    */
    void rollback() {
        auto newParent = originalParent;
        auto newIndex = indexInGroup;
        auto exParam = cast(ExParameter)self;
        if (exParam !is null) {
            originalParent = exParam.getParent();
            indexInGroup = originalParent? originalParent.children.countUntil(exParam): -1;
        }
        if (!added) {
            puppet.parameters ~= self;
            if (exParam !is null)
                exParam.setParent(newParent);
        } else {

            puppet.removeParameter(self);
        }
            
        // Re-apply drivers
        foreach(ref driver; drivers) {
            if (SimplePhysics sf = cast(SimplePhysics)driver) {
                sf.param = self;
            }
        }
        notifyParameterResourceChanged();
    }

    /**
        Redo
    */
    void redo() {
        auto newParent = originalParent;
        auto newIndex = indexInGroup;
        auto exParam = cast(ExParameter)self;
        if (exParam !is null) {
            originalParent = exParam.getParent();
            indexInGroup = originalParent? originalParent.children.countUntil(exParam): -1;
        }
        if (added) {
            puppet.parameters ~= self;
            if (exParam !is null)
                exParam.setParent(newParent);
        } else {
            puppet.removeParameter(self);
        }
            
        // Empty drivers
        foreach(ref driver; drivers) {
            if (SimplePhysics sf = cast(SimplePhysics)driver) {
                sf.param = null;
            }
        }
        notifyParameterResourceChanged();
    }

    /**
        Describe the action
    */
    string describe() {
        if (added)
            return _("Added parameter %s").format(self.name);
        else
            return _("Removed parameter %s").format(self.name);
    }

    /**
        Describe the action
    */
    string describeUndo() {
        if (added)
            return _("Parameter %s was removed").format(self.name);
        else
            return _("Parameter %s was added").format(self.name);
    }

    /**
        Gets name of this action
    */
    string getName() {
        return this.stringof;
    }
    
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }

private:
    void notifyParameterResourceChanged() {
        if (puppet !is null && puppet.root !is null)
            puppet.root.notifyChange(puppet.root, NotifyReason.StructureChanged);
    }
}

alias ParameterAddAction = ParameterAddRemoveAction!true;
alias ParameterRemoveAction = ParameterAddRemoveAction!false;


/**
    Action to remove parameter from active puppet.
*/
class ParameterValueChangeAction(T) : LazyBoundAction {
public:
    alias TSelf = typeof(this);
    string name;
    Parameter self;
    T oldValue;
    T newValue;
    T* valuePtr;

    this(string name, Parameter self, T oldValue, T newValue, T* valuePtr) {
        this.name     = name;
        this.self     = self;
        this.oldValue = oldValue;
        this.newValue = newValue;
        this.valuePtr = valuePtr;
    }

    this(string name, Parameter self, T* valuePtr, void delegate() update = null) {
        this.name     = name;
        this.self     = self;
        this.valuePtr = valuePtr;
        this.oldValue = *valuePtr;
        if (update !is null) {
            update();
            updateNewState();
        }
    }

    void updateNewState() {
        this.newValue = *valuePtr;
    }

    void clear() { }

    /**
        Rollback
    */
    void rollback() {
        *valuePtr = oldValue;
    }

    /**
        Redo
    */
    void redo() {
        *valuePtr = newValue;
    }

    /**
        Describe the action
    */
    string describe() {
        if (name == "axis points")
            return _("%s->%s changed").format(self.name, name);
        else
            return _("%s->%s changed to %s").format(self.name, name, newValue);
    }

    /**
        Describe the action
    */
    string describeUndo() {
        if (name == "axis points")
            return _("%s->%s change cancelled").format(self.name, name);
        else
            return _("%s->%s changed from %s").format(self.name, name, oldValue);
    }

    /**
        Gets name of this action
    */
    string getName() {
        return name;
    }
    
    /**
        Merge
    */
    bool merge(Action other) {
        if (this.canMerge(other)) {
            this.newValue = (cast(TSelf)other).newValue;
            return true;
        }
        return false;
    }

    /**
        Gets whether this node can merge with an other
    */
    bool canMerge(Action other) {
        TSelf otherChange = cast(TSelf) other;
        return (otherChange !is null && this.name == otherChange.name);
    }
}

class ParameterNameChangeAction : ParameterValueChangeAction!string {
public:
    alias TSelf = typeof(this);

    this(Parameter self, string oldValue, string newValue) {
        super("name", self, oldValue, newValue, &self.name_);
    }

    override
    void rollback() {
        super.rollback();
        self.makeIndexable();
    }

    override
    void redo() {
        super.redo();
        self.makeIndexable();
    }

    override
    bool canMerge(Action other) {
        auto otherChange = cast(TSelf)other;
        return otherChange !is null && self is otherChange.self;
    }
}

class ParameterGroupAddRemoveAction(bool added = true) : Action {
public:
    ExPuppet puppet;
    ExParameterGroup group;
    size_t index;
    ExParameter[] children;

    this(ExPuppet puppet, ExParameterGroup group, size_t index = size_t.max) {
        this.puppet = puppet;
        this.group = group;
        this.index = index == size_t.max ? puppet.groups.countUntil(group) : index;
        if (this.index == size_t.max)
            this.index = puppet.groups.length;
        foreach (child; group.children) {
            if (auto exChild = cast(ExParameter)child)
                children ~= exChild;
        }
    }

    private
    void insertGroup() {
        if (puppet.groups.countUntil(group) >= 0)
            return;
        auto insertIndex = index > puppet.groups.length ? puppet.groups.length : index;
        puppet.groups = puppet.groups[0 .. insertIndex] ~ group ~ puppet.groups[insertIndex .. $];
        foreach (child; children)
            child.setParent(group);
    }

    private
    void removeGroup() {
        foreach (child; group.children.dup) {
            if (auto exChild = cast(ExParameter)child)
                exChild.setParent(null);
        }
        auto currentIndex = puppet.groups.countUntil(group);
        if (currentIndex >= 0)
            puppet.groups = puppet.groups.remove(currentIndex);
    }

    void rollback() {
        static if (added)
            removeGroup();
        else
            insertGroup();
    }

    void redo() {
        static if (added)
            insertGroup();
        else
            removeGroup();
    }

    string describe() {
        static if (added)
            return _("Added parameter group %s").format(group.name);
        else
            return _("Removed parameter group %s").format(group.name);
    }

    string describeUndo() {
        static if (added)
            return _("Parameter group %s was removed").format(group.name);
        else
            return _("Parameter group %s was added").format(group.name);
    }

    string getName() {
        return this.stringof;
    }

    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

alias ParameterGroupAddAction = ParameterGroupAddRemoveAction!true;
alias ParameterGroupRemoveAction = ParameterGroupAddRemoveAction!false;

class ParameterMoveAction : Action {
public:
    ExParameter param;
    ExParameterGroup oldParent;
    ExParameterGroup newParent;

    this(Parameter param, ExParameterGroup oldParent, ExParameterGroup newParent) {
        this.param = cast(ExParameter)param;
        this.oldParent = oldParent;
        this.newParent = newParent;
    }

    void rollback() {
        if (param !is null)
            param.setParent(oldParent);
    }

    void redo() {
        if (param !is null)
            param.setParent(newParent);
    }

    string describe() {
        return _("%s moved").format(param !is null ? param.name : "");
    }

    string describeUndo() {
        return _("%s move cancelled").format(param !is null ? param.name : "");
    }

    string getName() {
        return this.stringof;
    }

    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

class ParameterShapeChangeAction : LazyBoundAction {
public:
    struct ValueBindingState {
        ValueParameterBinding binding;
        float[][] values;
        bool[][] isSet;
    }

    struct ParameterBindingState {
        ParameterParameterBinding binding;
        float[][] values;
        bool[][] isSet;
    }

    struct DeformationBindingState {
        DeformationParameterBinding binding;
        Deformation[][] values;
        bool[][] isSet;
    }

    struct State {
        vec2 min;
        vec2 max;
        float[][] axisPoints;
        ValueBindingState[] valueBindings;
        ParameterBindingState[] parameterBindings;
        DeformationBindingState[] deformationBindings;
    }

    string name;
    Parameter self;
    State oldState;
    State newState;

    this(string name, Parameter self) {
        this.name = name;
        this.self = self;
        oldState = captureState();
    }

    void updateNewState() {
        newState = captureState();
    }

    void clear() { }

    void rollback() {
        applyState(oldState);
    }

    void redo() {
        applyState(newState);
    }

    string describe() {
        return _("%s->%s changed").format(self.name, name);
    }

    string describeUndo() {
        return _("%s->%s change cancelled").format(self.name, name);
    }

    string getName() {
        return name;
    }

    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }

private:
    static float[][] dupFloatMatrix(float[][] source) {
        float[][] result;
        result.length = source.length;
        foreach (i, row; source)
            result[i] = row.dup;
        return result;
    }

    static bool[][] dupBoolMatrix(bool[][] source) {
        bool[][] result;
        result.length = source.length;
        foreach (i, row; source)
            result[i] = row.dup;
        return result;
    }

    static Deformation dupDeformation(Deformation source) {
        auto result = source;
        result.vertexOffsets = source.vertexOffsets.dup;
        return result;
    }

    static Deformation[][] dupDeformationMatrix(Deformation[][] source) {
        Deformation[][] result;
        result.length = source.length;
        foreach (i, row; source) {
            result[i].length = row.length;
            foreach (j, value; row)
                result[i][j] = dupDeformation(value);
        }
        return result;
    }

    State captureState() {
        State state;
        state.min = self.min;
        state.max = self.max;
        state.axisPoints = dupFloatMatrix(self.axisPoints);
        foreach (binding; self.bindings) {
            if (auto valueBinding = cast(ValueParameterBinding)binding) {
                state.valueBindings ~= ValueBindingState(valueBinding, dupFloatMatrix(valueBinding.values), dupBoolMatrix(valueBinding.isSet_));
            } else if (auto parameterBinding = cast(ParameterParameterBinding)binding) {
                state.parameterBindings ~= ParameterBindingState(parameterBinding, dupFloatMatrix(parameterBinding.values), dupBoolMatrix(parameterBinding.isSet_));
            } else if (auto deformationBinding = cast(DeformationParameterBinding)binding) {
                state.deformationBindings ~= DeformationBindingState(deformationBinding, dupDeformationMatrix(deformationBinding.values), dupBoolMatrix(deformationBinding.isSet_));
            }
        }
        return state;
    }

    void applyState(ref State state) {
        self.min = state.min;
        self.max = state.max;
        self.axisPoints = dupFloatMatrix(state.axisPoints);

        foreach (bindingState; state.valueBindings) {
            bindingState.binding.values = dupFloatMatrix(bindingState.values);
            bindingState.binding.isSet_ = dupBoolMatrix(bindingState.isSet);
            bindingState.binding.reInterpolate();
        }
        foreach (bindingState; state.parameterBindings) {
            bindingState.binding.values = dupFloatMatrix(bindingState.values);
            bindingState.binding.isSet_ = dupBoolMatrix(bindingState.isSet);
            bindingState.binding.reInterpolate();
        }
        foreach (bindingState; state.deformationBindings) {
            bindingState.binding.values = dupDeformationMatrix(bindingState.values);
            bindingState.binding.isSet_ = dupBoolMatrix(bindingState.isSet);
            bindingState.binding.reInterpolate();
        }
    }
}

/**
    Base class for actions to change multiple bindings of the same parameter at once.
*/
class AbstractParameterChangeBindingsAction(VarArg...) : GroupAction, LazyBoundAction {
public:
    alias TSelf = typeof(this);
    string name;
    Parameter self;

    this(string name, Parameter self, ParameterBinding[] bindings, Action function(ParameterBinding, VarArg) bindingActionMapper, VarArg args) {
        super([]);
        this.name     = name;
        this.self     = self;
        foreach (binding; (bindings !is null)? bindings: self.bindings) {
            Action action = bindingActionMapper(binding, args);
            if (action !is null) 
                addAction(action);
        }
    }

    override
    void updateNewState() {
        foreach (action; actions) {
            LazyBoundAction lazyAction = cast(LazyBoundAction)action;
            if (lazyAction !is null) 
                lazyAction.updateNewState();
        }
    }

    override
    void clear() {}

    /**
        Describe the action
    */
    override
    string describe() {
        return _("%s->%s changed").format(self.name, name);
    }

    /**
        Describe the action
    */
    override
    string describeUndo() {
        return _("%s->%s change cancelled").format(self.name, name);
    }

    /**
        Gets name of this action
    */
    override
    string getName() {
        return name;
    }
}


/**
    Actions to add bindings to parameter at once.
*/

Action BindingAddMapper(ParameterBinding binding, Parameter parent) {
    return new ParameterBindingAddAction(parent, binding);
}
class ParameterAddBindingsAction : AbstractParameterChangeBindingsAction!(Parameter) {
    this(string name, Parameter self, ParameterBinding[] bindings) {
        super(name, self, bindings, &BindingAddMapper, self);
    }
}


/**
    Actions to remove bindings from parameter at once.
*/

Action BindingRemoveMapper(ParameterBinding binding, Parameter parent) {
    return new ParameterBindingRemoveAction(parent, binding);
}
class ParameterRemoveBindingsAction : AbstractParameterChangeBindingsAction!(Parameter) {
    this(string name, Parameter self, ParameterBinding[] bindings) {
        super(name, self, bindings, &BindingRemoveMapper, self);
    }
}


/**
    Actions to change all binding values at once.
*/

Action BindingChangeMapper(ParameterBinding binding) {
    if (auto typedBinding = cast(ValueParameterBinding)binding) {
        return new ParameterBindingAllValueChangeAction!(float)(typedBinding.getName(), typedBinding);
    } else if (auto typedBinding = cast(DeformationParameterBinding)binding) {
        return new ParameterBindingAllValueChangeAction!(Deformation)(typedBinding.getName(), typedBinding);
    } else if (auto typedBinding = cast(ParameterParameterBinding)binding) {
        return new ParameterBindingAllValueChangeAction!(float)(typedBinding.getName(), typedBinding);
    } else {
        return null;
    }
}
class ParameterChangeBindingsAction : AbstractParameterChangeBindingsAction!() {
    this(string name, Parameter self, ParameterBinding[] bindings) {
        super(name, self, bindings, &BindingChangeMapper);
    }
}


/**
    Actions to change binding value of specified keypoints at once.
*/

Action BindingValueChangeMapper(ParameterBinding binding, int pointx, int pointy) {
    if (auto typedBinding = cast(ValueParameterBinding)binding) {
        return new ParameterBindingValueChangeAction!(float,typeof(typedBinding))(typedBinding.getName(), typedBinding, pointx, pointy);
    } else if (auto typedBinding = cast(DeformationParameterBinding)binding) {
        return new ParameterBindingValueChangeAction!(Deformation,typeof(typedBinding))(typedBinding.getName(), typedBinding, pointx, pointy);
    } else if (auto typedBinding = cast(ParameterParameterBinding)binding) {
        return new ParameterBindingValueChangeAction!(float,typeof(typedBinding))(typedBinding.getName(), typedBinding, pointx, pointy);
    } else {
        return null;
    }
}
class ParameterChangeBindingsValueAction : AbstractParameterChangeBindingsAction!(int, int) {
    this(string name, Parameter self, ParameterBinding[] bindings, int pointx, int pointy) {
        super(name, self, bindings, &BindingValueChangeMapper, pointx, pointy);
    }
}
