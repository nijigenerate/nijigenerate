module nijigenerate.commands.viewport.control;

import nijigenerate.commands.base;
import nijigenerate.viewport.model.onionslice;
import nijigenerate.viewport.base; // import only base to avoid package static ctor cycles
import nijigenerate.windows.flipconfig;
import nijigenerate.windows.automeshbatch;
import nijigenerate.windows;          // incPushWindow
import nijigenerate.widgets.modal;     // incModalAdd
import nijigenerate.ext;
import nijilive;
import i18n;

// Commands for viewport UI actions (buttons/menus)

class ToggleMirrorViewCommand : ExCommand!() {
    this() { super(_("Toggle mirror view.")); }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet) return CommandResult(false, "No puppet");
        incShouldMirrorViewport = !incShouldMirrorViewport;
        return CommandResult(true);
    }
}

class ToggleOnionSliceCommand : ExCommand!() {
    this() { super(_("Toggle onion slice overlay.")); }
    override CommandResult run(Context ctx) {
        auto onion = OnionSlice.singleton;
        onion.toggle();
        return CommandResult(true);
    }
}

class TogglePhysicsCommand : ExCommand!() {
    this() { super(_("Toggle physics drivers.")); }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return CommandResult(false, "No puppet");
        ctx.puppet.enableDrivers = !ctx.puppet.enableDrivers;
        return CommandResult(true);
    }
}

class TogglePostProcessCommand : ExCommand!() {
    this() { super(_("Toggle post processing.")); }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet) return CommandResult(false, "No puppet");
        incShouldPostProcess = !incShouldPostProcess;
        return CommandResult(true);
    }
}

class ResetPhysicsCommand : ExCommand!() {
    this() { super(_("Reset physics.")); }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return CommandResult(false, "No puppet");
        ctx.puppet.resetDrivers();
        return CommandResult(true);
    }
}

class ResetParametersCommand : ExCommand!() {
    this() { super(_("Reset parameters to defaults.")); }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return CommandResult(false, "No puppet");
        foreach (ref parameter; ctx.puppet.parameters) {
            parameter.value = parameter.defaults;
        }
        return CommandResult(true);
    }
}

class OpenFlipPairWindowCommand : ExCommand!() {
    this() { super(_("Open Flip Pair configuration window.")); }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet) return CommandResult(false, "No puppet");
        incPushWindow(new FlipPairWindow());
        return CommandResult(true);
    }
}

class OpenAutomeshBatchingCommand : ExCommand!() {
    this() { super(_("Open Automesh Batching modal.")); }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet) return CommandResult(false, "No puppet");
        incModalAdd(new AutoMeshBatchWindow());
        return CommandResult(true);
    }
}

class ResetViewportZoomCommand : ExCommand!() {
    this() { super(_("Reset viewport zoom to 1.0.")); }
    override CommandResult run(Context ctx) {
        incViewportTargetZoom = 1;
        return CommandResult(true);
    }
}

class ResetViewportPositionCommand : ExCommand!() {
    this() { super(_("Reset viewport position to origin.")); }
    override CommandResult run(Context ctx) {
        incViewportTargetPosition = vec2(0, 0);
        return CommandResult(true);
    }
}

enum ViewportCommand {
    ToggleMirrorView,
    ToggleOnionSlice,
    TogglePhysics,
    TogglePostProcess,
    ResetPhysics,
    ResetParameters,
    OpenFlipPairWindow,
    OpenAutomeshBatching,
    ResetViewportZoom,
    ResetViewportPosition,
}

Command[ViewportCommand] commands;

void ngInitCommands(T)() if (is(T == ViewportCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!ViewportCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}
