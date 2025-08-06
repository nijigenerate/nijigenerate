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


// 単一引数を文字列に変換するユーティリティ
template ArgToString(alias a) {
    static if (is(typeof(a) == bool))
        enum ArgToString = a.stringof;
    else static if (is(typeof(a) == InterpolateMode))
        enum ArgToString = "InterpolateMode." ~ a.stringof;
    else
        enum ArgToString = a.stringof;
}

// 複数引数をカンマ区切りに連結する
template ArgList(Args...) {
    static if (Args.length == 0)
        enum ArgList = "";
    else static if (Args.length == 1)
        enum ArgList = ArgToString!(Args[0]);
    else
        enum ArgList = ArgToString!(Args[0]) ~ ", " ~ ArgList!(Args[1 .. $]);
}

// コマンド登録用 mixin 定義（可変長引数対応）
template register(alias id, Args...) {
    import std.string : format;
    enum name = __traits(identifier, id);
    enum parentName = __traits(identifier, __traits(parent, id));
    enum ctor = name ~ "Command";
    static if (Args.length == 0) {
        enum register = format("commands[%s.%s] = new %s();", parentName, name, ctor);
    } else {
        enum argList = ArgList!Args;
        enum register = format("commands[%s.%s] = new %s(%s);", parentName, name, ctor, argList);
    }
}

// 引数なしで new できるかをチェック
template canDefaultConstruct(alias T) {
    enum canDefaultConstruct = __traits(compiles, new T());
}

// ParamCommand から FooCommand 型を生成
template GetCommandType(alias enumValue) {
    mixin("alias GetCommandType = " ~ enumValue.stringof ~ "Command;");
}

private {
    Command[ParamCommand] commands;

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!ParamCommand) {
            static if (canDefaultConstruct!(GetCommandType!name)) {
                mixin(register!(name));
            }
        }

        // テンプレート引数付きコマンドは直接インスタンス化で登録
        commands[ParamCommand.Add1DParameter] = new Add1DParameterCommand!(-1, 1);
        commands[ParamCommand.Add2DParameter] = new Add2DParameterCommand!(-1, 1);
        commands[ParamCommand.AddMouthParameter] = new AddMouthParameterCommand!(-1, 1);

        import std.stdio;
        writefln("\nparam");
        foreach (k, v; commands) {
            writefln("%s: %s", k, v);
        }
    }
}
