module nijigenerate.commands.node.welding;

import nijigenerate.commands.base;
import nijigenerate.actions;
import nijigenerate.panels.inspector.part : incRegisterWeldedPoints;
import nijigenerate.core.actionstack : incActionPush;
import nijilive; // Drawable
import std.algorithm.searching : countUntil;
import std.exception : enforce;
import i18n;

enum NodeWeldingCommand {
    AddWelding,
    RemoveWelding,
    ChangeWeldingWeight,
}

Command[NodeWeldingCommand] commands;

class AddWeldingCommand : ExCommand!(
    TW!(Drawable, "target", "Target drawable"),
    TW!(float, "weight", "Welding weight")
) {
    this(Drawable target, float weight) {
        super(_("Add Welding"), _("Add welding link"), target, weight);
    }
    override CommandResult run(Context ctx) {
        enforce(ctx.hasNodes && ctx.nodes.length > 0, "No source drawable in context");
        auto drawable = cast(Drawable)ctx.nodes[0];
        enforce(drawable !is null, "Context node is not a Drawable");
        incRegisterWeldedPoints(drawable, target, weight);
        return CommandResult(true);
    }
}

class RemoveWeldingCommand : ExCommand!(
    TW!(Drawable, "target", "Target drawable")
) {
    this(Drawable target) {
        super(_("Remove Welding"), _("Remove welding link"), target);
    }
    override CommandResult run(Context ctx) {
        enforce(ctx.hasNodes && ctx.nodes.length > 0, "No source drawable in context");
        auto drawable = cast(Drawable)ctx.nodes[0];
        enforce(drawable !is null, "Context node is not a Drawable");
        incActionPush(new DrawableRemoveWeldingAction(drawable, target, null, -1));
        return CommandResult(true);
    }
}

class ChangeWeldingWeightCommand : ExCommand!(
    TW!(Drawable, "target", "Target drawable"),
    TW!(float, "weight", "New welding weight")
) {
    this(Drawable target, float weight) {
        super(_("Change Welding Weight"), _("Change welding weight"), target, weight);
    }
    override CommandResult run(Context ctx) {
        enforce(ctx.hasNodes && ctx.nodes.length > 0, "No source drawable in context");
        auto drawable = cast(Drawable)ctx.nodes[0];
        enforce(drawable !is null, "Context node is not a Drawable");
        auto idx = drawable.welded.countUntil!((a)=>a.target == target)();
        enforce(idx >= 0, "Welding link not found");
        auto indices = drawable.welded[idx].indices;
        incActionPush(new DrawableChangeWeldingAction(drawable, target, indices, weight));
        return CommandResult(true);
    }
}

void ngInitCommands(T)() if (is(T == NodeWeldingCommand)) {
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!NodeWeldingCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    static foreach(name; EnumMembers!NodeWeldingCommand) {
        static if (name == NodeWeldingCommand.AddWelding) {
            static if (__traits(compiles, { mixin(registerCommand!(name, cast(Drawable)null, 0.5f)); }))
                mixin(registerCommand!(name, cast(Drawable)null, 0.5f));
        } else static if (name == NodeWeldingCommand.RemoveWelding) {
            static if (__traits(compiles, { mixin(registerCommand!(name, cast(Drawable)null)); }))
                mixin(registerCommand!(name, cast(Drawable)null));
        } else static if (name == NodeWeldingCommand.ChangeWeldingWeight) {
            static if (__traits(compiles, { mixin(registerCommand!(name, cast(Drawable)null, 0.5f)); }))
                mixin(registerCommand!(name, cast(Drawable)null, 0.5f));
        }
    }
    // Explicit registrations mirroring node.d style
    mixin(registerCommand!(NodeWeldingCommand.AddWelding, cast(Drawable)null, 0.5f));
    mixin(registerCommand!(NodeWeldingCommand.RemoveWelding, cast(Drawable)null));
    mixin(registerCommand!(NodeWeldingCommand.ChangeWeldingWeight, cast(Drawable)null, 0.5f));
}
