module nijigenerate.commands.node.mask;

import nijigenerate.commands.base;
import nijigenerate.actions;
import nijigenerate.core.actionstack : incActionPush;
import nijilive; // Part, Drawable, MaskingMode
import std.exception : enforce;
import i18n;

enum NodeMaskCommand {
    AddMask,
    RemoveMask,
    ChangeMaskMode,
}

Command[NodeMaskCommand] commands;

@EffectStructuralEdit
class AddMaskCommand : ExCommand!(
    TW!(Drawable, "maskSrc", "Mask source drawable"),
    TW!(MaskingMode, "mode", "Masking mode")
) {
    this(Drawable maskSrc, MaskingMode mode) {
        super(_("Add Mask"), _("Add mask source to part"), maskSrc, mode);
    }
    override CommandResult run(Context ctx) {
        enforce(ctx.hasNodes && ctx.nodes.length > 0, "No target part in context");
        auto target = cast(Part)ctx.nodes[0];
        enforce(target !is null, "Context node is not a Part");
        incActionPush(new PartAddMaskAction(maskSrc, target, mode));
        return CommandResult(true);
    }
}

@EffectDelete
class RemoveMaskCommand : ExCommand!(
    TW!(Drawable, "maskSrc", "Mask source drawable")
) {
    this(Drawable maskSrc) {
        super(_("Remove Mask"), _("Remove mask source from part"), maskSrc);
    }
    override CommandResult run(Context ctx) {
        enforce(ctx.hasNodes && ctx.nodes.length > 0, "No target part in context");
        auto target = cast(Part)ctx.nodes[0];
        enforce(target !is null, "Context node is not a Part");
        auto idx = target.getMaskIdx(maskSrc);
        enforce(idx >= 0, "Mask entry not found");
        auto mode = target.masks[idx].mode;
        incActionPush(new PartRemoveMaskAction(maskSrc, target, mode));
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class ChangeMaskModeCommand : ExCommand!(
    TW!(Drawable, "maskSrc", "Mask source drawable"),
    TW!(MaskingMode, "mode", "New masking mode")
) {
    this(Drawable maskSrc, MaskingMode mode) {
        super(_("Change Mask Mode"), _("Change mask mode"), maskSrc, mode);
    }
    override CommandResult run(Context ctx) {
        enforce(ctx.hasNodes && ctx.nodes.length > 0, "No target part in context");
        auto target = cast(Part)ctx.nodes[0];
        enforce(target !is null, "Context node is not a Part");
        incActionPush(new PartChangeMaskModeAction(target, maskSrc, mode));
        return CommandResult(true);
    }
}

void ngInitCommands(T)() if (is(T == NodeMaskCommand)) {
    auto addMaskCommand = new AddMaskCommand(null, MaskingMode.Mask);
    ngRegisterCommandMeta(addMaskCommand);
    commands[NodeMaskCommand.AddMask] = addMaskCommand;

    auto removeMaskCommand = new RemoveMaskCommand(null);
    ngRegisterCommandMeta(removeMaskCommand);
    commands[NodeMaskCommand.RemoveMask] = removeMaskCommand;

    auto changeMaskModeCommand = new ChangeMaskModeCommand(null, MaskingMode.Mask);
    ngRegisterCommandMeta(changeMaskModeCommand);
    commands[NodeMaskCommand.ChangeMaskMode] = changeMaskModeCommand;
}
