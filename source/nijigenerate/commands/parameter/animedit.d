module nijigenerate.commands.parameter.animedit;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.project;

class AddKeyFrameCommand : ExCommand!() {
    this() { super("Add KeyFrame"); }

    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];

                if (param.isVec2) {
                    incAnimationKeyframeAdd(param, 0, param.value.vector[0]);
                    incAnimationKeyframeAdd(param, 1, param.value.vector[1]);
                } else {
                    incAnimationKeyframeAdd(param, 0, param.value.vector[0]);
                }
            }
        }
    }
}

enum AnimeditCommand {
    AddKeyFrame
}

import std.meta : staticMap;
import std.array : join;
import std.string : format;

template ArgsToString(Args...) {
    static if (is(Args == bool))
        enum ArgsToString = "";
    else
        enum ArgsToString = staticMap!(a => a.stringof, Args).join(", ");
}

template registerCommand(EnumType, CommandArray, alias id, Args...) {
    enum ctor = id.stringof ~ "Command";
    static if (is(Args == bool))
        enum registerCommand = format(`%s[%s.%s] = new %s();`, CommandArray, EnumType.stringof, id.stringof, ctor);
    else
        enum registerCommand = format(`%s[%s.%s] = new %s(%s);`, CommandArray, EnumType.stringof, id.stringof, ctor, ArgsToString!Args);
}

Command[AnimeditCommand] commands;
private {

    // 引数なしで new できるかをチェック
    template canDefaultConstruct(T) {
        enum canDefaultConstruct = __traits(compiles, new T());
    }

    // AnimeditCommand から FooCommand 型を生成
    template GetCommandType(alias enumValue) {
        mixin("alias GetCommandType = " ~ enumValue.stringof ~ "Command;");
    }

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!AnimeditCommand) {
            static if (__traits(compiles, { mixin(registerCommand!(name)); }))
                mixin(registerCommand!(commands, name));
        }
    }
}
