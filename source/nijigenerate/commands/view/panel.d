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

<<<<<<< HEAD
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
=======
// Unique key type for panel commands (avoids generic types like string)
struct PanelKey {
    string name;
    // For AllCommandMaps ID generation
    string toString() const { return name; }
    // Hashing for AA key
    size_t toHash() const @safe nothrow @nogc { import core.internal.hash : hashOf; return hashOf(name); }
    // Equality for AA key
    bool opEquals(const PanelKey rhs) const @safe nothrow @nogc { return name == rhs.name; }
}

// Dynamic registry keyed by PanelKey (stable, distinct type)
Command[PanelKey] togglePanelCommands;

/// Ensure a toggle command exists for a panel
Command ensureTogglePanelCommand(Panel p)
{
    if (p is null) return null;
    PanelKey key = PanelKey(p.name());
    auto found = key in togglePanelCommands;
    if (found) return *found;
    auto c = cast(Command) new TogglePanelVisibilityCommand();
    (cast(TogglePanelVisibilityCommand)c).panel = p;
    togglePanelCommands[key] = c;
    return c;
}

// Pre-populate panel toggle commands at startup (called via ngInitAllCommands)
void ngInitCommands(T)() if (is(T == PanelKey))
{
    foreach (p; incPanels) {
        ensureTogglePanelCommand(p);
>>>>>>> ed133c9 (feat(shortcuts): move shortcut editor to Settings and add dynamic Panel toggle commands\n\n- Add commands.view.panel with PanelKey + dynamic AA and ngInitCommands to register all panels.\n- Integrate panel toggles into Settings shortcut editor and View menu using commands.\n- Remove separate shortcut window; keep editing under Settings as required.)
    }
}
