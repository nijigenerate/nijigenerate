module nijigenerate.commands.parameter.animedit;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.project;
import nijigenerate.core.actionstack : incActionPushGroup, incActionPopGroup;
import nijilive;
import i18n;

private struct AnimationKeyframeClipboardEntry {
    int axis;
    float value;
}

private AnimationKeyframeClipboardEntry[] animationKeyframeClipboard;

private bool copyAnimationKeyframesAtCurrentFrame(Parameter param) {
    auto playback = incAnimationGet();
    if (playback is null || playback.animation is null || param is null)
        return false;

    AnimationKeyframeClipboardEntry[] copied;
    foreach (ref lane; playback.animation.lanes) {
        if (lane.paramRef.targetParam != param)
            continue;
        foreach (frame; lane.frames) {
            if (frame.frame == playback.frame) {
                copied ~= AnimationKeyframeClipboardEntry(lane.paramRef.targetAxis, frame.value);
                break;
            }
        }
    }

    if (copied.length == 0)
        return false;
    animationKeyframeClipboard = copied;
    return true;
}

private bool pasteAnimationKeyframesAtCurrentFrame(Parameter param) {
    if (animationKeyframeClipboard.length == 0 || incAnimationGet() is null || param is null)
        return false;

    bool changed;
    incActionPushGroup();
    scope(exit) incActionPopGroup();
    foreach (entry; animationKeyframeClipboard) {
        if (entry.axis < 0 || entry.axis >= param.value.vector.length)
            continue;
        incAnimationKeyframeAdd(param, entry.axis, entry.value);
        changed = true;
    }
    return changed;
}

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
                    incActionPushGroup();
                    scope(exit) incActionPopGroup();
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

@EffectKeyframeEdit
class CopyAnimationKeyFrameCommand : ExCommand!() {
    this() {
        super(
            _("Animation: Copy Keyframe"),
            _("Copy the selected parameter's keyframe at the current animation frame.")
        );
    }

    override bool runnable(Context ctx) {
        return incEditMode() == EditMode.AnimEdit
            && incAnimationGet() !is null
            && ctx.hasParameters()
            && ctx.parameters.length != 0
            && ctx.parameters[0] !is null;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx))
            return CommandResult(false, "Animation Edit mode, an active animation, and a parameter are required");
        return copyAnimationKeyframesAtCurrentFrame(ctx.parameters[0])
            ? CommandResult(true)
            : CommandResult(false, "No keyframe at current frame");
    }
}

@EffectKeyframeEdit
class PasteAnimationKeyFrameCommand : ExCommand!() {
    this() {
        super(
            _("Animation: Paste Keyframe"),
            _("Paste copied animation keyframe values to the selected parameter at the current animation frame.")
        );
    }

    override bool runnable(Context ctx) {
        return incEditMode() == EditMode.AnimEdit
            && incAnimationGet() !is null
            && ctx.hasParameters()
            && ctx.parameters.length != 0
            && ctx.parameters[0] !is null
            && animationKeyframeClipboard.length != 0;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx))
            return CommandResult(false, "Animation Edit mode, an active animation, a parameter, and copied keyframes are required");
        return pasteAnimationKeyframesAtCurrentFrame(ctx.parameters[0])
            ? CommandResult(true)
            : CommandResult(false, "No compatible copied keyframes");
    }
}

enum AnimeditCommand {
    AddAnimationKeyFrame,
    CopyAnimationKeyFrame,
    PasteAnimationKeyFrame
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
