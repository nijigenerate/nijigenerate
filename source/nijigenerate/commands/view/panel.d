module nijigenerate.commands.view.panel;

import nijigenerate.commands.base;
import nijigenerate.panels; // Panel, incPanels
import nijigenerate.core.settings;
import i18n;

/// Toggle visibility for a given Panel instance.
class TogglePanelVisibilityCommand : ExCommand!(TW!(Panel, "panel", "target panel to toggle")) {
    this() { super(_("Toggle Panel"), cast(Panel)null); }

    override string label() {
        return panel ? panel.displayName() : _label;
    }

    override bool runnable(Context ctx) {
        return panel !is null && !panel.alwaysVisible && panel.isActive();
    }

    override void run(Context ctx) {
        if (panel is null || panel.alwaysVisible) return;
        panel.visible = !panel.visible;
        incSettingsSet(panel.name ~ ".visible", panel.visible);
    }
}

enum PanelMenuCommand {
    TogglePanelVisibility,
}

Command[PanelMenuCommand] commands;

void ngInitCommands(T)() if (is(T == PanelMenuCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!PanelMenuCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}
