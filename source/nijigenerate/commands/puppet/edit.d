module nijigenerate.commands.puppet.edit;

import nijigenerate.commands.base;
import nijilive;
import nijigenerate.core.actionstack;
import nijigenerate.windows;           // incPushWindow
import nijigenerate.windows.settings;  // SettingsWindow class
import i18n;

class UndoCommand : ExCommand!() {
    this() { super(_("Undo"), _("Undo last action")); }
    override
    void run(Context ctx) {
        incActionUndo();
    }
    override bool runnable(Context ctx) { return incActionCanUndo(); }
}

class RedoCommand : ExCommand!() {
    this() { super(_("Redo"), _("Redo previously undone action")); }
    override
    void run(Context ctx) {
        incActionRedo();
    }
    override bool runnable(Context ctx) { return incActionCanRedo(); }
}

class ShowSettingsWindowCommand : ExCommand!() {
    this() { super(_("Settings"), _("Show settings window")); }
    override
    void run(Context ctx) {
        if (!incIsSettingsOpen) incPushWindow(new SettingsWindow);
    }
}


enum EditCommand {
    Undo,
    Redo,
    ShowSettingsWindow,
}


Command[EditCommand] commands;

void ngInitCommands(T)() if (is(T == EditCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!EditCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}
