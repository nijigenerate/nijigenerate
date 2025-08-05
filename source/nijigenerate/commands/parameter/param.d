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

class Add1DParameterCommand(int min, int max) : ExCommand!() {
    this() { super("Add 1D Parameter (%d..%d)".format(min, max)); }
    override
    void run(Context ctx) {
        if (!ctx.hasPuppet)
            return;
        
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
    }
}

class Add2DParameterCommand(int min, int max) : ExCommand!() {
    this() { super("Add 2D Parameter (%d..%d)".format(min, max)); }
    override
    void run(Context ctx) {
        if (!ctx.hasPuppet)
            return;
        
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
    }
}

class AddMouthParameterCommand(int min, int max) : ExCommand!() {
    this() { super("Add Mouth Parameter (%d..%d)".format(min, max)); }
    override
    void run(Context ctx) {
        if (!ctx.hasPuppet)
            return;
        
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
    }
}

class RemoveParameterCommand : ExCommand!() {
    this() { super("Remove Parameter"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters) return;
        foreach (param; ctx.parameters)
            removeParameter(param);
    }
}

enum ParamCommand {
    Add1DParameter,
    Add2DParameter,
    AddMouthParameter,
    RemoveParameter
}


// コマンド登録用 mixin 定義（文字列引数方式）
template register(alias id, string args = "") {
    import std.string : format;
    enum name = __traits(identifier, id);
    enum parentName = __traits(identifier, __traits(parent, id));
    enum ctor = name ~ "Command";
    static if (args.length == 0)
        enum register = format("commands[%s.%s] = new %s();",
                               parentName, name, ctor);
    else
        enum register = format("commands[%s.%s] = new %s(%s);",
                               parentName, name, ctor, args);
}

private {
    Command[ParamCommand] commands;

    static this() {
        import std.traits : EnumMembers;
        // 自動登録：コンパイル可能なコマンドのみ mixin
        static foreach (name; EnumMembers!ParamCommand) {
            static if (__traits(compiles, mixin(register!(name)))) {
                mixin(register!(name));
            }
        }
        // テンプレート引数付きコマンドは直接インスタンス化で登録
        commands[ParamCommand.Add1DParameter] = new Add1DParameterCommand!(-1, 1);
        commands[ParamCommand.Add2DParameter] = new Add2DParameterCommand!(-1, 1);
        commands[ParamCommand.AddMouthParameter] = new AddMouthParameterCommand!(-1, 1);
    }
}
