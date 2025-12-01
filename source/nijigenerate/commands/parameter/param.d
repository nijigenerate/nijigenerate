module nijigenerate.commands.parameter.param;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijilive;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.windows;
import nijigenerate.core;
import nijigenerate.project;
import nijigenerate.actions;
import i18n;
import std.format : format;

class Add1DParameterCommand : ExCommand!(TW!(int, "min", "minimum value of the Parameter"), TW!(int, "max", "maximum value of the Parameter")) {
    this(int min, int max) { super(null, _("Add 1D Parameter (%d..%d)").format(min, max), min, max); }
    override
    CreateResult!Parameter run(Context ctx) {
        if (!ctx.hasPuppet)
            return new CreateResult!Parameter(false, null, "No puppet");
        
        Parameter param = new ExParameter(
            "Param #%d\0".format(ctx.parameters.length),
            false
        );
        param.min.x = min;
        param.max.x = max;
        if (min + max == 0)
        param.insertAxisPoint(0, 0.5);
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
        auto res = new CreateResult!Parameter(true, [param], "Parameter created");
        return res;
    }
}

class Add2DParameterCommand : ExCommand!(TW!(int, "min", "minimum value of the Parameter"), TW!(int, "max", "maximum value of the Parameter")) {
    this(int min, int max) { super(null, _("Add 2D Parameter (%d..%d)").format(min, max), min, max); }
    override
    CreateResult!Parameter run(Context ctx) {
        if (!ctx.hasPuppet)
            return new CreateResult!Parameter(false, null, "No puppet");
        
        Parameter param = new ExParameter(
            "Param #%d\0".format(ctx.parameters.length),
            true
        );
        param.min = vec2(min, min);
        param.max = vec2(max, max);
        if (min + max == 0) {
            param.insertAxisPoint(0, 0.5);
            param.insertAxisPoint(1, 0.5);
        }
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
        auto res = new CreateResult!Parameter(true, [param], "Parameter created");
        return res;
    }
}

class AddMouthParameterCommand : ExCommand!() {
    this() { super(null, _("Add Mouth Parameter")); }
    override
    CreateResult!Parameter run(Context ctx) {
        if (!ctx.hasPuppet)
            return new CreateResult!Parameter(false, null, "No puppet");
        
        Parameter param = new ExParameter(
            "Mouth #%d\0".format(ctx.parameters.length),
            true
        );
        param.min = vec2(-1, 0);
        param.max = vec2(1, 1);
        param.insertAxisPoint(0, 0.25);
        param.insertAxisPoint(0, 0.5);
        param.insertAxisPoint(0, 0.6);
        param.insertAxisPoint(1, 0.3);
        param.insertAxisPoint(1, 0.5);
        param.insertAxisPoint(1, 0.6);
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
        auto res = new CreateResult!Parameter(true, [param], "Parameter created");
        return res;
    }
}

class RemoveParameterCommand : ExCommand!() {
    this() { super(null, _("Remove selected parameters from the active puppet")); }
    override
    DeleteResult!Parameter run(Context ctx) {
        if (!ctx.hasParameters) return new DeleteResult!Parameter(false, null, "No parameters");
        foreach (param; ctx.parameters)
            removeParameter(param);
        auto res = new DeleteResult!Parameter(true, ctx.parameters.dup, "Parameters removed");
        return res;
    }
}

enum ParamCommand {
    Add1DParameter,
    Add2DParameter,
    AddMouthParameter,
    RemoveParameter
}


Command[ParamCommand] commands;

void ngInitCommands(T)() if (is(T == ParamCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!ParamCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    mixin(registerCommand!(ParamCommand.Add1DParameter, -1, 1));
    mixin(registerCommand!(ParamCommand.Add2DParameter, -1, 1));
}
