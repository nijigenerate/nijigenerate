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
    CommandResult run(Context ctx) {
        incActionUndo();
        return CommandResult(true);
    }
    override bool runnable(Context ctx) { return incActionCanUndo(); }
}

class RedoCommand : ExCommand!() {
    this() { super(_("Redo"), _("Redo previously undone action")); }
    override
    CommandResult run(Context ctx) {
        incActionRedo();
        return CommandResult(true);
    }
    override bool runnable(Context ctx) { return incActionCanRedo(); }
}

class ShowSettingsWindowCommand : ExCommand!() {
    this() { super(_("Settings"), _("Show settings window")); }
    override
    CommandResult run(Context ctx) {
        if (!incIsSettingsOpen) incPushWindow(new SettingsWindow);
        return CommandResult(true);
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
