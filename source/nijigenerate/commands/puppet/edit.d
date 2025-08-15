module nijigenerate.commands.puppet.edit;

import nijigenerate.commands.base;
import nijilive;
import nijigenerate.core.actionstack;
import nijigenerate.windows;

class UndoCommand : ExCommand!() {
    this() { super("Undo", "Undo last action"); }
    override
    void run(Context ctx) {
        incActionUndo();
    }
}

class RedoCommand : ExCommand!() {
    this() { super("Redo", "Redo previously undone action"); }
    override
    void run(Context ctx) {
        incActionRedo();
    }
}

class ShowSettingsWindowCommand : ExCommand!() {
    this() { super("Settings", "Show settings window"); }
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
private {

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!EditCommand) {
            static if (__traits(compiles, { mixin(registerCommand!(name)); }))
                mixin(registerCommand!(name));
        }

//        mixin(registerCommand!(NodeCommand.MoveNode, null, 0));
    }
}
