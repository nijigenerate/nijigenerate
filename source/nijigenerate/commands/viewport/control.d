module nijigenerate.commands.viewport.control;

import nijigenerate.commands.base;
import nijigenerate.viewport.model.onionslice;
import nijigenerate.viewport; // for incViewportTargetZoom/Position and related globals
import nijigenerate.windows.flipconfig;
import nijigenerate.windows.automeshbatch;
import nijigenerate.windows;          // incPushWindow
import nijigenerate.widgets.modal;     // incModalAdd
import nijigenerate.ext;
import nijilive;

// Commands for viewport UI actions (buttons/menus)

class ToggleMirrorViewCommand : ExCommand!() {
    this() { super("Toggle mirror view."); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incShouldMirrorViewport = !incShouldMirrorViewport;
    }
}

class ToggleOnionSliceCommand : ExCommand!() {
    this() { super("Toggle onion slice overlay."); }
    override void run(Context ctx) {
        auto onion = OnionSlice.singleton;
        onion.toggle();
    }
}

class TogglePhysicsCommand : ExCommand!() {
    this() { super("Toggle physics drivers."); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return;
        ctx.puppet.enableDrivers = !ctx.puppet.enableDrivers;
    }
}

class TogglePostProcessCommand : ExCommand!() {
    this() { super("Toggle post processing."); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incShouldPostProcess = !incShouldPostProcess;
    }
}

class ResetPhysicsCommand : ExCommand!() {
    this() { super("Reset physics."); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return;
        ctx.puppet.resetDrivers();
    }
}

class ResetParametersCommand : ExCommand!() {
    this() { super("Reset parameters to defaults."); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return;
        foreach (ref parameter; ctx.puppet.parameters) {
            parameter.value = parameter.defaults;
        }
    }
}

class OpenFlipPairWindowCommand : ExCommand!() {
    this() { super("Open Flip Pair configuration window."); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incPushWindow(new FlipPairWindow());
    }
}

class OpenAutomeshBatchingCommand : ExCommand!() {
    this() { super("Open Automesh Batching modal."); }
    override void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        incModalAdd(new AutoMeshBatchWindow());
    }
}

class ResetViewportZoomCommand : ExCommand!() {
    this() { super("Reset viewport zoom to 1.0."); }
    override void run(Context ctx) {
        incViewportTargetZoom = 1;
    }
}

class ResetViewportPositionCommand : ExCommand!() {
    this() { super("Reset viewport position to origin."); }
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
private {
    static this() {
        import std.traits : EnumMembers;
        static foreach (name; EnumMembers!ViewportCommand) {
            static if (__traits(compiles, { mixin(registerCommand!(name)); }))
                mixin(registerCommand!(name));
        }
    }
}
