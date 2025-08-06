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
    enum ctor = id.stringof ~ "Command";
    static if (Args.length == 0) {
        enum register = format(`commands[AnimeditCommand.%s] = new %s();`, id.stringof, ctor);
    } else {
        enum argList = ArgList!Args;
        enum register = format(`commands[AnimeditCommand.%s] = new %s(%s);`, id.stringof, ctor, argList);
    }
}

private {
    Command[AnimeditCommand] commands;

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
            static if (canDefaultConstruct!(GetCommandType!name)) {
                mixin(register!(name));
            }
        }

        // 引数ありのコンストラクタを持つコマンドは手動登録
        // 例: mixin(register!(AnimeditCommand.SomeCommand, true));

        import std.stdio;
        writefln("\nanimedit");
        foreach (k, v; commands) {
            writefln("%s: %s", k, v);
        }
    }
}
