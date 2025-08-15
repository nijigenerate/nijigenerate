module nijigenerate.core.shortcut;

import nijigenerate.commands; // Command, Context and command maps/enums
import nijigenerate.core.input; // incShortcut
import nijigenerate.project; // active/selection state
import bindbc.imgui;

// Optional imports to enrich Context at execution time
import nijigenerate.panels.parameters : incParamPoint; // key point
import nijigenerate.commands.binding.base : cSelectedBindings; // selected bindings map

// Action entry representing a shortcut binding to a Command
struct ActionEntry {
    string shortcut;    // e.g. "Ctrl+Shift+P"
    Command command;    // Bound command instance
    bool repeat;        // Whether to allow key repeat
}

// Registry keyed by shortcut string (as used by incShortcut)
private ActionEntry[string] gShortcutEntries;

// Register or overwrite a shortcut (public API)
void ngRegisterShortcut(string shortcut, Command command, bool repeat = false)
{
    gShortcutEntries[shortcut] = ActionEntry(shortcut, command, repeat);
}

// Find shortcut string for a given command instance (empty if none)
string ngShortcutFor(Command cmd)
{
    import std.algorithm : any;
    foreach (k, entry; gShortcutEntries) {
        if (entry.command is cmd) return entry.shortcut;
    }
    return "";
}

// Build a Context and populate as much as possible from current app state
private Context buildExecutionContext()
{
    Context ctx = new Context();

    // Puppet
    ctx.puppet = incActivePuppet();

    // Selected nodes (only set when exists to keep masks consistent)
    auto selNodes = incSelectedNodes();
    if (selNodes.length > 0)
        ctx.nodes = selNodes;

    // Armed parameter, if any
    auto armed = incArmedParameter();
    if (armed !is null)
        ctx.parameters = [armed];

    // Selected bindings if any (from binding panel context)
    import std.array : array;
    auto bindings = cSelectedBindings.byValue().array;
    if (bindings.length > 0)
        ctx.bindings = bindings;

    // Current key point for parameter editing
    ctx.keyPoint = incParamPoint();

    return ctx;
}

// Initialize default shortcuts mapped to existing Command instances
private void registerDefaultShortcuts()
{
    // File
    ngRegisterShortcut(_K!"Ctrl-N",
        nijigenerate.commands.puppet.file.commands[FileCommand.NewFile]);
    ngRegisterShortcut(_K!"Ctrl-O",
        nijigenerate.commands.puppet.file.commands[FileCommand.ShowOpenFileDialog]);
    ngRegisterShortcut(_K!"Ctrl-S",
        nijigenerate.commands.puppet.file.commands[FileCommand.ShowSaveFileDialog]);
    ngRegisterShortcut(_K!"Ctrl-Shift-S",
        nijigenerate.commands.puppet.file.commands[FileCommand.ShowSaveFileAsDialog]);

    // Edit
    ngRegisterShortcut(_K!"Ctrl-Shift-Z",
        nijigenerate.commands.puppet.edit.commands[EditCommand.Redo], true);
    ngRegisterShortcut(_K!"Ctrl-Z",
        nijigenerate.commands.puppet.edit.commands[EditCommand.Undo], true);

    // Node
    ngRegisterShortcut(_K!"Ctrl-X",
        nijigenerate.commands.node.node.commands[NodeCommand.CutNode], true);
    ngRegisterShortcut(_K!"Ctrl-C",
        nijigenerate.commands.node.node.commands[NodeCommand.CopyNode], true);
    ngRegisterShortcut(_K!"Ctrl-V",
        nijigenerate.commands.node.node.commands[NodeCommand.PasteNode], true);
}

static this()
{
    registerDefaultShortcuts();
}

// Handle shortcut inputs each frame: find first matching and execute
void incHandleShortcuts()
{
    auto io = igGetIO();
    foreach (k, entry; gShortcutEntries) {
        if (incShortcut(entry.shortcut, entry.repeat)) {
            auto ctx = buildExecutionContext();
            entry.command.run(ctx);
            break; // handle one per frame, closest match wins
        }
    }
}
