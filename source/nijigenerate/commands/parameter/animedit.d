module nijigenerate.commands.parameter.animedit;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.project;
import i18n;

@EffectKeyframeEdit
class AddAnimationKeyFrameCommand : ExCommand!() {
    this() {
        super(
            _("Animation: Add Keyframe"),
            _("Add a keyframe for the selected parameter in the active animation. Requires Animation Edit mode and an active animation.")
        );
    }

    override bool runnable(Context ctx) {
        return incEditMode() == EditMode.AnimEdit
            && incAnimationGet() !is null
            && ctx.hasParameters()
            && ctx.parameters.length != 0
            && ctx.parameters[0] !is null;
    }

    override
    CommandResult run(Context ctx) {
        if (!runnable(ctx)) {
            if (incEditMode() != EditMode.AnimEdit)
                return CommandResult(false, "Animation Edit mode is required");
            if (incAnimationGet() is null)
                return CommandResult(false, "No active animation");
            return CommandResult(false, "No parameter");
        }
        if (incAnimationGet() is null) {
            return CommandResult(false, "No active animation");
        }
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];
                if (param is null) {
                    return CommandResult(false, "No parameter");
                }

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
    AddAnimationKeyFrame
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
