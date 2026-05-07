module nijigenerate.commands.parameter.paramedit;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.commands.binding.base;
import nijilive;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.viewport.model.deform;
import nijigenerate.ext;
import nijigenerate.project;
import i18n;
import std.algorithm.searching : countUntil;


//                    incPushWindowList(new ParamPropWindow(param));
//                    incPushWindowList(new ParamAxesWindow(param));
//                    incPushWindowList(new ParamSplitWindow(idx, param));

@EffectStructuralEdit
class ConvertTo2DParamCommand : ExCommand!() {
    this() { super(_("Convert to 2D")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            convertTo2D(param);
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class FlipXCommand : ExCommand!() {
    this() { super(_("Flip X")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            auto action = new ParameterChangeBindingsAction("Flip X", param, null);
            param.reverseAxis(0);
            action.updateNewState();
            incActionPush(action);
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class FlipYCommand : ExCommand!() {
    this() { super(_("Flip X")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            auto action = new ParameterChangeBindingsAction("Flip Y", param, null);
            param.reverseAxis(1);
            action.updateNewState();
            incActionPush(action);
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class Flip1DCommand : ExCommand!() {
    this() { super(_("Flip")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            auto action = new ParameterChangeBindingsAction("Flip", param, null);
            param.reverseAxis(0);
            action.updateNewState();
            incActionPush(action);
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class MirrorHorizontallyCommand : ExCommand!() {
    this() { super(_("Mirror Horizontally")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            mirrorAll(param, 0);
            incViewportNodeDeformNotifyParamValueChanged();
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class MirrorVerticallyCommand : ExCommand!() {
    this() { super(_("Mirror Vertically")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            mirrorAll(param, 1);
            incViewportNodeDeformNotifyParamValueChanged();
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class MirroredAutoFillDir1Command : ExCommand!() {
    this() { super(_("Mirrored Autofill ")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            mirroredAutofill(param, 0, 0, 0.4999);
            incViewportNodeDeformNotifyParamValueChanged();
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class MirroredAutoFillDir2Command : ExCommand!() {
    this() { super(_("Mirrored Autofill ")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            mirroredAutofill(param, 0, 0.5001, 1);
            incViewportNodeDeformNotifyParamValueChanged();
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class MirroredAutoFillDir3Command : ExCommand!() {
    this() { super(_("Mirrored Autofill ")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            mirroredAutofill(param, 1, 0.5001, 1);
            incViewportNodeDeformNotifyParamValueChanged();
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class MirroredAutoFillDir4Command : ExCommand!() {
    this() { super(_("Mirrored Autofill ")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        foreach (param; ctx.parameters) {
            mirroredAutofill(param, 1, 0, 0.4999);
            incViewportNodeDeformNotifyParamValueChanged();
        }
        return CommandResult(true);
    }
}

class CopyParameterCommand : ExCommand!() {
    this() { super(_("Copy Parameter")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        auto param = ctx.parameters[0];
        cClipboardParameter = param.dup;
        return CommandResult(true);
    }
}

@EffectCreate
class PasteParameterCommand : ExCommand!() {
    this() { super(_("Paste Parameter")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        auto param = ctx.parameters[0];
        pasteParameter(param, null, 2);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
    override bool runnable(Context ctx) {
        import nijigenerate.commands.binding.base : cClipboardParameter;
        return ctx.hasParameters && ctx.parameters.length > 0 && cClipboardParameter !is null;
    }
}

@EffectCreate
class PasteParameterWithFlipCommand : ExCommand!() {
    this() { super(_("Paste Parameter with Flip")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        auto param = ctx.parameters[0];
        pasteParameter(param, null, 0);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
    override bool runnable(Context ctx) {
        import nijigenerate.commands.binding.base : cClipboardParameter;
        return ctx.hasParameters && ctx.parameters.length > 0 && cClipboardParameter !is null;
    }
}

@EffectCreate
class DuplicateParameterCommand : ExCommand!() {
    this() { super(_("Duplicate Parameter")); }
    override
    CreateResult!Parameter run(Context ctx) {
        if (!(ctx.hasPuppet && ctx.hasParameters) || ctx.parameters.length == 0) return new CreateResult!Parameter(false, null, "No parameters");
        auto param = ctx.parameters[0];

        Parameter newParam = param.dup;
        ctx.puppet.parameters ~= newParam;
        if (auto exParam = cast(ExParameter)newParam) {
            exParam.setParent((cast(ExParameter)param).getParent());
        }
        incActionPush(new ParameterAddAction(newParam));
        auto res = new CreateResult!Parameter(true, [newParam], "Parameter duplicated");
        return res;
    }
}

@EffectCreate
class DuplicateParameterWithFlipCommand : ExCommand!() {
    this() { super(_("Duplicate Parameter with Flip")); }
    override
    CreateResult!Parameter run(Context ctx) {
        if (!(ctx.hasPuppet && ctx.hasParameters) || ctx.parameters.length == 0) return new CreateResult!Parameter(false, null, "No parameters");
        auto param = ctx.parameters[0];

        Parameter newParam = param.dup;
        ctx.puppet.parameters ~= newParam;
        newParam.bindings.length = 0;
        pasteParameter!false(newParam, param, 0);
        if (auto exParam = cast(ExParameter)newParam) {
            exParam.setParent((cast(ExParameter)param).getParent());
        }
        incActionPush(new ParameterAddAction(newParam, cast(Parameter[]*)[])); //parentList is not used. so passed [].
        auto res = new CreateResult!Parameter(true, [newParam], "Parameter duplicated with flip");
        return res;
    }
}

@EffectDelete
class DeleteParameterCommand : ExCommand!() {
    this() { super(_("Delete Parameter")); }
    override
    DeleteResult!Parameter run(Context ctx) {
        if (!(ctx.hasPuppet && ctx.hasParameters) || ctx.parameters.length == 0) return new DeleteResult!Parameter(false, null, "No parameters");
        auto param = ctx.parameters[0];

        if (incArmedParameter() == param) {
            incDisarmParameter();
        }
        if (incParamInSelection(param)) {
            incRemoveSelectParam(param);
        }
        incActionPush(new ParameterRemoveAction(param));
        ctx.puppet.removeParameter(param);
        return new DeleteResult!Parameter(true, [param], "Parameter deleted");
    }
}

void addBinding(Parameter param, Parameter p, int fromAxis, int toAxis) {
    auto binding = param.createBinding(p, toAxis);
    param.addBinding(binding);
//                    writefln("%s", param.bindings);
    auto ppBinding = cast(ParameterParameterBinding)binding;
    vec2 pos;
    vec2u index;

    pos = vec2(0, 0);
    pos.vector[fromAxis] = param.min.vector[fromAxis];
    index = param.findClosestKeypoint(pos);
    ppBinding.values[index.vector[0]][index.vector[1]] = p.min.vector[toAxis];
    ppBinding.isSet_[index.vector[0]][index.vector[1]] = true;

    pos = vec2(0, 0);
    pos.vector[fromAxis] = param.max.vector[fromAxis];
    index = param.findClosestKeypoint(pos);
    ppBinding.values[index.vector[0]][index.vector[1]] = p.max.vector[toAxis];
    ppBinding.isSet_[index.vector[0]][index.vector[1]] = true;

    ppBinding.reInterpolate();
    auto action = new ParameterBindingAddAction(param, binding);
    incActionPush(action);
}

@EffectStructuralEdit
class LinkToCommand : ExCommand!(TW!(Parameter, "toParam", "Target parameter to copy"), TW!(int, "fromAxis", "axis in source parameter"), TW!(int, "toAxis", "axis in dest parameter")) {
    this(Parameter toParam, int fromAxis, int toAxis) { super(null, _("Link To Parameter"), toParam, fromAxis, toAxis); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        auto param = ctx.parameters[0];
        addBinding(param, toParam, fromAxis, toAxis);
        return CommandResult(true);
    }
}


@EffectStructuralEdit
class ToggleParameterArmCommand : ExCommand!(TW!(int, "index", "specify the index of the armed parameter in the parent group.")) {
    this(int index) { super(null, _("Arm/Disarm the selected parameter at index"), index); }
    override
    CommandResult run(Context ctx) {
        if (!(ctx.hasPuppet && ctx.hasParameters) || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        auto param = ctx.parameters[0];
 
        if (incArmedParameter() == param) {
            incDisarmParameter();
        } else {
            param.value = param.getClosestKeypointValue();
            paramPointChanged(param);
            incArmParameter(index, param);
        }
        return CommandResult(true);
    }
}

private bool findParameterIndex(Parameter param, out size_t index) {
    if (param is null) return false;

    if (auto exParam = cast(ExParameter)param) {
        if (auto parent = exParam.getParent()) {
            auto found = parent.children.countUntil(param);
            if (found >= 0) {
                index = cast(size_t)found;
                return true;
            }
        }
    }

    if (auto puppet = incActivePuppet()) {
        auto found = puppet.parameters.countUntil(param);
        if (found >= 0) {
            index = cast(size_t)found;
            return true;
        }
    }
    return false;
}

@ShortcutHidden
class SetParameterKeypointCommand : ExCommand!() {
    this() {
        super(
            _("Set Parameter Keypoint"),
            _("Set context.parameters[0] to context.parameterValue without changing bindings or GUI ArmedParameter. Use SetArmedParameterAndKeypoint when the command should also arm the parameter.")
        );
    }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0)
            return CommandResult(false, "SetParameterKeypoint requires context.parameters[0]");
        auto param = ctx.parameters[0];

        vec2u keyPoint;
        vec2 resolved;
        auto result = resolveParameterKeypointValue(ctx, param, keyPoint, resolved);
        if (!result.succeeded) return result;

        return applyParameterKeypoint(param, keyPoint, resolved, false);
    }
}

@ShortcutHidden
class SetArmedParameterAndKeypointCommand : ExCommand!() {
    this() {
        super(
            _("Set Armed Parameter and Keypoint"),
            _("Set the target parameter to context.parameterValue and make it the GUI ArmedParameter. Prefer context.armedParameters[0]; if omitted, uses the current ArmedParameter. Use SetParameterKeypoint when no GUI ArmedParameter change is desired.")
        );
    }

    override
    CommandResult run(Context ctx) {
        Parameter param = null;
        if (ctx.hasArmedParameters && ctx.armedParameters.length > 0)
            param = ctx.armedParameters[0];
        else
            param = incArmedParameter();
        if (param is null) return CommandResult(false, "No parameter");

        vec2u keyPoint;
        vec2 resolved;

        auto result = resolveParameterKeypointValue(ctx, param, keyPoint, resolved);
        if (!result.succeeded) return result;

        return applyParameterKeypoint(param, keyPoint, resolved, true);
    }
}

private CommandResult resolveParameterKeypointValue(Context ctx, Parameter param, out vec2u keyPoint, out vec2 resolved) {
    if (param is null) return CommandResult(false, "No parameter");

    if (ctx.hasParameterValue) {
        keyPoint = param.findClosestKeypoint(ctx.parameterValue);
        if (keyPoint.x >= param.axisPointCount(0) || keyPoint.y >= param.axisPointCount(1))
            return CommandResult(false, "context.parameterValue resolved keypoint is out of range");

        resolved = param.getKeypointValue(keyPoint);
        import std.math : abs;
        enum float epsilon = 1e-5f;
        if (abs(resolved.x - ctx.parameterValue.x) > epsilon || abs(resolved.y - ctx.parameterValue.y) > epsilon)
            return CommandResult(false, "context.parameterValue does not match an existing key value");
    } else if (ctx.hasKeyPoint && ctx.hasExplicitKeyPoint) {
        keyPoint = ctx.keyPoint;
        if (keyPoint.x >= param.axisPointCount(0) || keyPoint.y >= param.axisPointCount(1))
            return CommandResult(false, "keyPoint is out of range for parameter");
        resolved = param.getKeypointValue(keyPoint);
    } else {
        return CommandResult(false, "context.parameterValue is required");
    }

    return CommandResult(true);
}

private CommandResult applyParameterKeypoint(Parameter param, vec2u keyPoint, vec2 resolved, bool armParameter) {
    size_t index;
    if (!findParameterIndex(param, index))
        return CommandResult(false, "Parameter not found in active puppet");

    if (incEditMode() != EditMode.ModelEdit)
        incSetEditMode(EditMode.ModelEdit, false);

    param.value = resolved;
    paramPointChanged(param);
    if (armParameter)
        incArmParameter(index, param);
    incViewportNodeDeformNotifyParamValueChanged();
    return CommandResult(true);
}

@EffectKeyframeEdit
class SetStartingKeyFrameCommand : ExCommand!() {
    this() { super(null, _("Set the current parameter value as starting keyframe")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");
        auto param = ctx.parameters[0];

        auto action = new ParameterValueChangeAction!vec2("axis points", param, &param.defaults);
        param.defaults = param.value;
        action.updateNewState();
        incActionPush(action); 
        return CommandResult(true);
    }
}

enum ParameditCommand {
    ConvertTo2DParam,
    FlipX,
    FlipY,
    Flip1D,
    MirrorHorizontally,
    MirrorVertically,
    MirroredAutoFillDir1,
    MirroredAutoFillDir2,
    MirroredAutoFillDir3,
    MirroredAutoFillDir4,
    CopyParameter,
    PasteParameter,
    PasteParameterWithFlip,
    DuplicateParameter,
    DuplicateParameterWithFlip,
    DeleteParameter,
    LinkTo,
    ToggleParameterArm,
    SetParameterKeypoint,
    SetArmedParameterAndKeypoint,
    SetStartingKeyFrame
}


import nijigenerate.commands.base : registerCommand;

Command[ParameditCommand] commands;

void ngInitCommands(T)() if (is(T == ParameditCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!ParameditCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    mixin(registerCommand!(ParameditCommand.LinkTo, cast(Parameter)null, 0, 0));
    mixin(registerCommand!(ParameditCommand.ToggleParameterArm, 0));
}
