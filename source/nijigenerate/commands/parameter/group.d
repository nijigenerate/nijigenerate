module nijigenerate.commands.parameter.group;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijilive;
import nijigenerate.ext;
import nijigenerate.ext.param;
import nijigenerate.widgets;
import nijigenerate.windows;
import nijigenerate.core;
import nijigenerate.project;
import i18n;
import std.array : insertInPlace;


//==================================================================================
// Command Palette Definition for Parameter Group
//==================================================================================


class MoveParameterCommand : ExCommand!(ExParameterGroup, int) {
    this(ExParameterGroup group, int index) { super("Move Parameter", group, index);}
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return;

        incMoveParameter(ctx.parameters[0], arg0, arg1);
    }
}

class CreateParamGroupCommand : ExCommand!(int) {
    this(int index = 0) { super("Create Parameter Group", index); }
    override
    void run(Context ctx) {

        if (!ctx.hasPuppet) return;

//        if (index < 0) index = 0;
//        else if (index > ctx.puppet.parameters.length) 
//            index = cast(int)ctx.puppet.parameters.length-1;

        auto group = new ExParameterGroup(_("New Parameter Group"));
        (cast(ExPuppet)ctx.puppet).addGroup(group);
    }
}

class ChangeGroupColorCommand : ExCommand!(vec3) {
    this(vec3 color) { super("Change Parameter Group Color", color); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length < 1 || (cast(ExParameterGroup)ctx.parameters[0]) is null)
            return;
        auto group = cast(ExParameterGroup)ctx.parameters[0];
        group.color = arg0;
    }
}

class DeleteParamGroupCommand : ExCommand!() {
    this() { super("Delete Parameter Group"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length < 1 || (cast(ExParameterGroup)ctx.parameters[0]) is null)
            return;
        auto group = cast(ExParameterGroup)ctx.parameters[0];

        foreach(child; group.children) {
            auto exChild = cast(ExParameter)child;
            exChild.setParent(null);
        }
        (cast(ExPuppet)incActivePuppet()).removeGroup(group);
    }
}

enum GroupCommand {
    MoveParameter,
    CreateParamGroup,
    ChangeGroupColor,
    DeleteParamGroup
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

// GroupCommand から FooCommand 型を生成
template GetCommandType(alias enumValue) {
    mixin("alias GetCommandType = " ~ enumValue.stringof ~ "Command;");
}

private {
    Command[GroupCommand] commands;

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!GroupCommand) {
            static if (canDefaultConstruct!(GetCommandType!name)) {
                mixin(register!(name));
            }
        }

        // 引数付きコンストラクタを持つコマンドは明示的に登録
        mixin(register!(GroupCommand.MoveParameter, null, 0));
        mixin(register!(GroupCommand.CreateParamGroup, 0));
        auto color = vec3(0, 0, 0);
        mixin(register!(GroupCommand.ChangeGroupColor, color));

        import std.stdio;
        writefln("\ngroup");
        foreach (k, v; commands) {
            writefln("%s: %s", k, v);
        }
    }
}
