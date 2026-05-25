module nijigenerate.commands.node.simplephysics;

import nijigenerate;
import nijigenerate.actions;
import nijigenerate.commands.base;
import nijigenerate.core.actionstack : incActionPush;
import nijilive;
import nijilive.core.nodes.drivers;
import std.exception : enforce;
import std.traits : EnumMembers;
import i18n;

enum NodeSimplePhysicsCommand {
    SetSimplePhysicsParameter,
    ClearSimplePhysicsParameter,
    SetSimplePhysicsModelType,
    SetSimplePhysicsMapMode,
    SetSimplePhysicsLocalOnly,
    SetSimplePhysicsGravity,
    SetSimplePhysicsLength,
    SetSimplePhysicsFrequency,
    SetSimplePhysicsAngleDamping,
    SetSimplePhysicsLengthDamping,
    SetSimplePhysicsOutputScaleX,
    SetSimplePhysicsOutputScaleY,
}

Command[NodeSimplePhysicsCommand] commands;

private SimplePhysics[] simplePhysicsTargets(Context ctx) {
    enforce(ctx.hasNodes && ctx.nodes.length > 0, "No SimplePhysics node in context");
    SimplePhysics[] targets;
    foreach (node; ctx.nodes) {
        if (auto physics = cast(SimplePhysics)node) targets ~= physics;
    }
    enforce(targets.length > 0, "Context does not contain a SimplePhysics node");
    return targets;
}

private void notifySimplePhysics(SimplePhysics node, bool rescan = false) {
    node.notifyChange(node, NotifyReason.AttributeChanged);
    if (rescan) incActivePuppet().rescanNodes();
}

private Parameter getPhysicsParam(SimplePhysics node) {
    return node.param;
}

private PhysicsModel getPhysicsModelType(SimplePhysics node) {
    return node.modelType;
}

private ParamMapMode getPhysicsMapMode(SimplePhysics node) {
    return node.mapMode;
}

private bool getPhysicsLocalOnly(SimplePhysics node) {
    return node.localOnly;
}

private float getPhysicsGravity(SimplePhysics node) {
    return node.gravity;
}

private float getPhysicsLength(SimplePhysics node) {
    return node.length;
}

private float getPhysicsFrequency(SimplePhysics node) {
    return node.frequency;
}

private float getPhysicsAngleDamping(SimplePhysics node) {
    return node.angleDamping;
}

private float getPhysicsLengthDamping(SimplePhysics node) {
    return node.lengthDamping;
}

private float getPhysicsOutputScaleX(SimplePhysics node) {
    return node.outputScale.vector[0];
}

private float getPhysicsOutputScaleY(SimplePhysics node) {
    return node.outputScale.vector[1];
}

private void setPhysicsParam(SimplePhysics node, Parameter value) {
    node.param = value;
}

private void setPhysicsModelType(SimplePhysics node, PhysicsModel value) {
    node.modelType = value;
}

private void setPhysicsMapMode(SimplePhysics node, ParamMapMode value) {
    node.mapMode = value;
}

private void setPhysicsLocalOnly(SimplePhysics node, bool value) {
    node.localOnly = value;
}

private void setPhysicsGravity(SimplePhysics node, float value) {
    node.gravity = value;
}

private void setPhysicsLength(SimplePhysics node, float value) {
    node.length = value;
}

private void setPhysicsFrequency(SimplePhysics node, float value) {
    node.frequency = value;
}

private void setPhysicsAngleDamping(SimplePhysics node, float value) {
    node.angleDamping = value;
}

private void setPhysicsLengthDamping(SimplePhysics node, float value) {
    node.lengthDamping = value;
}

private void setPhysicsOutputScaleX(SimplePhysics node, float value) {
    node.outputScale.vector[0] = value;
}

private void setPhysicsOutputScaleY(SimplePhysics node, float value) {
    node.outputScale.vector[1] = value;
}

class SimplePhysicsValueChangeAction(T, alias Getter, alias Setter, bool rescan = false) : Action {
    string propName;
    SimplePhysics[] nodes;
    T[] oldValues;
    T newValue;

    this(string propName, SimplePhysics[] nodes, T newValue) {
        this.propName = propName;
        this.nodes = nodes.dup;
        this.newValue = newValue;
        foreach (node; nodes) oldValues ~= Getter(node);
        redo();
    }

    override void rollback() {
        foreach (i, node; nodes) {
            Setter(node, oldValues[i]);
            notifySimplePhysics(node, rescan);
        }
    }

    override void redo() {
        foreach (node; nodes) {
            Setter(node, newValue);
            notifySimplePhysics(node, rescan);
        }
    }

    override string describe() {
        return _("Changed SimplePhysics %s").format(propName);
    }

    override string describeUndo() {
        return _("Undo SimplePhysics %s change").format(propName);
    }

    override string getName() {
        return "SimplePhysics." ~ propName;
    }

    override bool canMerge(Action other) {
        static if (is(T == Parameter))
            return false;
        auto o = cast(SimplePhysicsValueChangeAction!(T, Getter, Setter, rescan))other;
        if (o is null) return false;
        if (o.propName != propName) return false;
        if (o.nodes.length != nodes.length) return false;
        foreach (i, node; nodes) {
            if (node.uuid != o.nodes[i].uuid) return false;
        }
        return true;
    }

    override bool merge(Action other) {
        auto o = cast(SimplePhysicsValueChangeAction!(T, Getter, Setter, rescan))other;
        if (o is null || !canMerge(other)) return false;
        newValue = o.newValue;
        return true;
    }
}

private bool simplePhysicsValueWouldChange(T, alias Getter)(SimplePhysics[] nodes, T newValue) {
    if (nodes.length == 0)
        return false;
    foreach (node; nodes) {
        if (Getter(node) != newValue)
            return true;
    }
    return false;
}

@ShortcutHidden
@EffectStructuralEdit
class SetSimplePhysicsParameterCommand : ExCommand!(
    TW!(Parameter, "parameter", "Target parameter driven by this SimplePhysics node")
) {
    this(Parameter parameter) {
        super(_("Set SimplePhysics Parameter"), _("Assign the parameter driven by the target SimplePhysics node"), parameter);
    }

    override CommandResult run(Context ctx) {
        auto targets = simplePhysicsTargets(ctx);
        if (!simplePhysicsValueWouldChange!(Parameter, getPhysicsParam)(targets, parameter))
            return CommandResult(false, "SimplePhysics parameter is unchanged");
        incActionPush(new SimplePhysicsValueChangeAction!(Parameter, getPhysicsParam, setPhysicsParam, true)(
            "parameter",
            targets,
            parameter
        ));
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectStructuralEdit
class ClearSimplePhysicsParameterCommand : ExCommand!() {
    this() {
        super(_("Clear SimplePhysics Parameter"), _("Unassign the parameter driven by the target SimplePhysics node"));
    }

    override CommandResult run(Context ctx) {
        auto targets = simplePhysicsTargets(ctx);
        if (!simplePhysicsValueWouldChange!(Parameter, getPhysicsParam)(targets, null))
            return CommandResult(false, "SimplePhysics parameter is unchanged");
        incActionPush(new SimplePhysicsValueChangeAction!(Parameter, getPhysicsParam, setPhysicsParam, true)(
            "parameter",
            targets,
            null
        ));
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectConfigEdit
class SetSimplePhysicsModelTypeCommand : ExCommand!(
    TW!(PhysicsModel, "modelType", "Physics model type")
) {
    this(PhysicsModel modelType) {
        super(_("Set SimplePhysics Model Type"), _("Set SimplePhysics model type"), modelType);
    }

    override CommandResult run(Context ctx) {
        incActionPush(new SimplePhysicsValueChangeAction!(PhysicsModel, getPhysicsModelType, setPhysicsModelType)(
            "modelType",
            simplePhysicsTargets(ctx),
            modelType
        ));
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectConfigEdit
class SetSimplePhysicsMapModeCommand : ExCommand!(
    TW!(ParamMapMode, "mapMode", "Parameter mapping mode")
) {
    this(ParamMapMode mapMode) {
        super(_("Set SimplePhysics Map Mode"), _("Set SimplePhysics parameter mapping mode"), mapMode);
    }

    override CommandResult run(Context ctx) {
        incActionPush(new SimplePhysicsValueChangeAction!(ParamMapMode, getPhysicsMapMode, setPhysicsMapMode)(
            "mapMode",
            simplePhysicsTargets(ctx),
            mapMode
        ));
        return CommandResult(true);
    }
}

@ShortcutHidden
@EffectConfigEdit
class SetSimplePhysicsLocalOnlyCommand : ExCommand!(
    TW!(bool, "localOnly", "Whether physics listens only to local transform")
) {
    this(bool localOnly) {
        super(_("Set SimplePhysics Local Only"), _("Set whether SimplePhysics listens only to local transform"), localOnly);
    }

    override CommandResult run(Context ctx) {
        incActionPush(new SimplePhysicsValueChangeAction!(bool, getPhysicsLocalOnly, setPhysicsLocalOnly)(
            "localOnly",
            simplePhysicsTargets(ctx),
            localOnly
        ));
        return CommandResult(true);
    }
}

mixin template SimplePhysicsFloatCommand(
    string ClassName,
    string Label,
    string Description,
    string PropertyName,
    alias Getter,
    alias Setter
) {
    mixin(`
        @ShortcutHidden
        @EffectConfigEdit
        class ` ~ ClassName ~ ` : ExCommand!(TW!(float, "value", "` ~ Description ~ `")) {
            this(float value) {
                super(_("` ~ Label ~ `"), _("` ~ Description ~ `"), value);
            }

            override CommandResult run(Context ctx) {
                incActionPush(new SimplePhysicsValueChangeAction!(float, Getter, Setter)(
                    "` ~ PropertyName ~ `",
                    simplePhysicsTargets(ctx),
                    value
                ));
                return CommandResult(true);
            }
        }
    `);
}

mixin SimplePhysicsFloatCommand!(
    "SetSimplePhysicsGravityCommand",
    "Set SimplePhysics Gravity",
    "Set SimplePhysics gravity scale",
    "gravity",
    getPhysicsGravity,
    setPhysicsGravity
);

mixin SimplePhysicsFloatCommand!(
    "SetSimplePhysicsLengthCommand",
    "Set SimplePhysics Length",
    "Set SimplePhysics pendulum or spring rest length",
    "length",
    getPhysicsLength,
    setPhysicsLength
);

mixin SimplePhysicsFloatCommand!(
    "SetSimplePhysicsFrequencyCommand",
    "Set SimplePhysics Frequency",
    "Set SimplePhysics resonant frequency",
    "frequency",
    getPhysicsFrequency,
    setPhysicsFrequency
);

mixin SimplePhysicsFloatCommand!(
    "SetSimplePhysicsAngleDampingCommand",
    "Set SimplePhysics Angle Damping",
    "Set SimplePhysics angular damping ratio",
    "angleDamping",
    getPhysicsAngleDamping,
    setPhysicsAngleDamping
);

mixin SimplePhysicsFloatCommand!(
    "SetSimplePhysicsLengthDampingCommand",
    "Set SimplePhysics Length Damping",
    "Set SimplePhysics length damping ratio",
    "lengthDamping",
    getPhysicsLengthDamping,
    setPhysicsLengthDamping
);

mixin SimplePhysicsFloatCommand!(
    "SetSimplePhysicsOutputScaleXCommand",
    "Set SimplePhysics Output Scale X",
    "Set SimplePhysics output scale X",
    "outputScale.x",
    getPhysicsOutputScaleX,
    setPhysicsOutputScaleX
);

mixin SimplePhysicsFloatCommand!(
    "SetSimplePhysicsOutputScaleYCommand",
    "Set SimplePhysics Output Scale Y",
    "Set SimplePhysics output scale Y",
    "outputScale.y",
    getPhysicsOutputScaleY,
    setPhysicsOutputScaleY
);

void ngInitCommands(T)() if (is(T == NodeSimplePhysicsCommand)) {
    commands[NodeSimplePhysicsCommand.SetSimplePhysicsParameter] = new SetSimplePhysicsParameterCommand(null);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsParameter]);

    commands[NodeSimplePhysicsCommand.ClearSimplePhysicsParameter] = new ClearSimplePhysicsParameterCommand();
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.ClearSimplePhysicsParameter]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsModelType] = new SetSimplePhysicsModelTypeCommand(PhysicsModel.Pendulum);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsModelType]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsMapMode] = new SetSimplePhysicsMapModeCommand(ParamMapMode.AngleLength);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsMapMode]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsLocalOnly] = new SetSimplePhysicsLocalOnlyCommand(false);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsLocalOnly]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsGravity] = new SetSimplePhysicsGravityCommand(1.0f);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsGravity]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsLength] = new SetSimplePhysicsLengthCommand(100.0f);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsLength]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsFrequency] = new SetSimplePhysicsFrequencyCommand(1.0f);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsFrequency]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsAngleDamping] = new SetSimplePhysicsAngleDampingCommand(0.5f);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsAngleDamping]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsLengthDamping] = new SetSimplePhysicsLengthDampingCommand(0.5f);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsLengthDamping]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsOutputScaleX] = new SetSimplePhysicsOutputScaleXCommand(1.0f);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsOutputScaleX]);

    commands[NodeSimplePhysicsCommand.SetSimplePhysicsOutputScaleY] = new SetSimplePhysicsOutputScaleYCommand(1.0f);
    ngRegisterCommandMeta(commands[NodeSimplePhysicsCommand.SetSimplePhysicsOutputScaleY]);
}
