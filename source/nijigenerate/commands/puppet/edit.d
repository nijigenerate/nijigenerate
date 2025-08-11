module nijigenerate.commands.puppet.edit;

import nijigenerate.commands.base;
import nijilive;
import nijigenerate.core.actionstack;

class UndoCommand : ExCommand!() {
    this() { super("Undo"); }
    override
    void run(Context ctx) {
        incActionUndo();
    }
}

class RedoCommand : ExCommand!() {
    this() { super("Redo"); }
    override
    void run(Context ctx) {
        incActionRedo();
    }
}


enum EditCommand {
    Undo,
    Redo
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
