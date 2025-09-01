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
    }
}
