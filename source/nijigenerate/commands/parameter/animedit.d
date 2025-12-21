module nijigenerate.commands.parameter.animedit;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.project;

class AddKeyFrameCommand : ExCommand!() {
    this() { super("Add KeyFrame"); }

    override
    CommandResult run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];

                if (param.isVec2) {
                    incAnimationKeyframeAdd(param, 0, param.value.vector[0]);
                    incAnimationKeyframeAdd(param, 1, param.value.vector[1]);
                } else {
                    incAnimationKeyframeAdd(param, 0, param.value.vector[0]);
                }
                return CommandResult(true);
            }
        }
        return CommandResult(false, "No parameters");
    }
}

enum AnimeditCommand {
    AddKeyFrame
}

import std.meta : staticMap;
import std.array : join;
import std.string : format;

Command[AnimeditCommand] commands;

void ngInitCommands(T)() if (is(T == AnimeditCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!AnimeditCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}
