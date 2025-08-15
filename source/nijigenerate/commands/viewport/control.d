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
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incShouldMirrorViewport = !incShouldMirrorViewport;
    }
}

class ToggleOnionSliceCommand : ExCommand!() {
    this() { super(_("Toggle onion slice overlay.")); }
    override void run(Context ctx) {
        auto onion = OnionSlice.singleton;
        onion.toggle();
    }
}

class TogglePhysicsCommand : ExCommand!() {
    this() { super(_("Toggle physics drivers.")); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return;
        ctx.puppet.enableDrivers = !ctx.puppet.enableDrivers;
    }
}

class TogglePostProcessCommand : ExCommand!() {
    this() { super(_("Toggle post processing.")); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incShouldPostProcess = !incShouldPostProcess;
    }
}

class ResetPhysicsCommand : ExCommand!() {
    this() { super(_("Reset physics.")); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return;
        ctx.puppet.resetDrivers();
    }
}

class ResetParametersCommand : ExCommand!() {
    this() { super(_("Reset parameters to defaults.")); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return;
        foreach (ref parameter; ctx.puppet.parameters) {
            parameter.value = parameter.defaults;
        }
    }
}

class OpenFlipPairWindowCommand : ExCommand!() {
    this() { super(_("Open Flip Pair configuration window.")); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incPushWindow(new FlipPairWindow());
    }
}

class OpenAutomeshBatchingCommand : ExCommand!() {
    this() { super(_("Open Automesh Batching modal.")); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incModalAdd(new AutoMeshBatchWindow());
    }
}

class ResetViewportZoomCommand : ExCommand!() {
    this() { super(_("Reset viewport zoom to 1.0.")); }
    override void run(Context ctx) {
        incViewportTargetZoom = 1;
    }
}

class ResetViewportPositionCommand : ExCommand!() {
    this() { super(_("Reset viewport position to origin.")); }
    override void run(Context ctx) {
        incViewportTargetPosition = vec2(0, 0);
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
