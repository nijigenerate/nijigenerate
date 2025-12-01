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


//                    incPushWindowList(new ParamPropWindow(param));
//                    incPushWindowList(new ParamAxesWindow(param));
//                    incPushWindowList(new ParamSplitWindow(idx, param));

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

class DeleteParameterCommand : ExCommand!() {
    this() { super(_("Delete Parameter")); }
    override
    DeleteResult!Parameter run(Context ctx) {
        if (!(ctx.hasPuppet && ctx.hasParameters) || ctx.parameters.length == 0) return new DeleteResult!Parameter(false, null, "No parameters");
        auto param = ctx.parameters[0];

        if (ctx.puppet == param) {
            incDisarmParameter();
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


class ToggleParameterArmCommand : ExCommand!(TW!(int, "index", "specify the index of the armed parameter in the parent group.")) {
    this(int index) { super(null, _("Toggle Armed Parameter"), index); }
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

class SetStartingKeyFrameCommand : ExCommand!() {
    this() { super(null, _("Set Starting KeyFrame")); }
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
