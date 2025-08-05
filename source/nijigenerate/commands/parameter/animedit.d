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
template ArgsToString(Args...) {
    static if (is(Args == bool))
        enum ArgsToString = "";
    else
        enum ArgsToString = staticMap!(a => a.stringof, Args).join(", ");
}

// Helper template for command registration
template register(alias id, Args...) {
    import std.string : format;
    enum ctor = id.stringof ~ "Command";
    static if (is(Args == bool))
        enum register = format(`commands[AnimeditCommand.%s] = new %s();`, id.stringof, ctor);
    else
        enum register = format(`commands[AnimeditCommand.%s] = new %s(%s);`, id.stringof, ctor, ArgsToString!Args);
}

private {
    Command[AnimeditCommand] commands;

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!AnimeditCommand) {
            static if (__traits(compiles, mixin(register!(name))))
                mixin(register!(name));
        }
    }
}
