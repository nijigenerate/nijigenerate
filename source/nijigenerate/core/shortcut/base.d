module nijigenerate.core.shortcut.base;

// Keep this module free of command/UI imports. It provides infrastructure only.
import nijigenerate.commands.base : Command, Context; // base types
import nijigenerate.core.input;            // incShortcut
import nijigenerate.project;               // active/selection state
import bindbc.imgui;
import inmath : vec2u;
import nijilive; // ParameterBinding
import std.json;                  // persist shortcuts
import std.conv : to;
import nijigenerate.core.settings; // settings store

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

// Remove any existing shortcut(s) bound to the given command instance
void ngClearShortcutFor(Command command)
{
    import std.array : array;
    auto keys = gShortcutEntries.byKey.array;
    foreach (k; keys) {
        auto e = gShortcutEntries[k];
        if (e.command is command) {
            gShortcutEntries.remove(k);
        }
    }
}

// List all registered action entries (for debugging/UI)
ActionEntry[] ngListShortcuts()
{
    import std.array : array;
    return gShortcutEntries.byValue.array;
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

// Optional providers (set by other modules) to enrich Context without hard imports
private vec2u function() gParamPointProvider;
private ParameterBinding[] function() gSelectedBindingsProvider;

void ngSetParamPointProvider(vec2u function() provider)
{
    gParamPointProvider = provider;
}

void ngSetSelectedBindingsProvider(ParameterBinding[] function() provider)
{
    gSelectedBindingsProvider = provider;
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

    // Selected bindings if any (from binding panel context), via provider
    if (gSelectedBindingsProvider !is null) {
        auto bindings = gSelectedBindingsProvider();
        if (bindings.length > 0)
            ctx.bindings = bindings;
    }

    // Current key point for parameter editing (via provider)
    if (gParamPointProvider !is null)
        ctx.keyPoint = gParamPointProvider();

    return ctx;
}

// Handle shortcut inputs each frame: find first matching and execute
void incHandleShortcuts()
{
    auto io = igGetIO();
    foreach (k, entry; gShortcutEntries) {
        if (incShortcut(entry.shortcut, entry.repeat)) {
            auto ctx = buildExecutionContext();
            if (entry.command.runnable(ctx))
                entry.command.run(ctx);
            break; // handle one per frame, closest match wins
        }
    }
}

// ===== Persistence (save/load) =====

// Build a map from "EnumType.ValueName" to Command instance across all registered command maps
private Command[string] _buildCommandIdMap()
{
    Command[string] res;
    import nijigenerate.commands : AllCommandMaps;

    static foreach (AA; AllCommandMaps) {
        foreach (k, v; AA) {
            string id = typeof(k).stringof ~ "." ~ to!string(k);
            res[id] = v;
        }
    }
    return res;
}

// Get identifier string for a given registered command instance (empty if not found)
private string _commandIdFor(Command cmd)
{
    auto all = _buildCommandIdMap();
    foreach (id, c; all) {
        if (c is cmd) return id;
    }
    return "";
}

// Save current shortcuts to settings as an object map: { "Type.Value": "Shortcut" }
void ngSaveShortcutsToSettings()
{
    JSONValue[string] obj; // JSON object
    foreach (k, entry; gShortcutEntries) {
        auto id = _commandIdFor(entry.command);
        if (id.length) obj[id] = JSONValue(entry.shortcut);
    }
    incSettingsSet("Shortcuts", JSONValue(obj));
}

// Load shortcuts from settings and register them, overriding existing bindings
void ngLoadShortcutsFromSettings()
{
    if (!incSettingsCanGet("Shortcuts")) return;
    auto j = incSettingsGet!JSONValue("Shortcuts");
    if (j.type != JSON_TYPE.OBJECT) return;

    auto all = _buildCommandIdMap();
    foreach (key, val; j.object) {
        if (auto p = key in all) {
            auto sc = val.type == JSON_TYPE.STRING ? val.str : null;
            if (sc.length) {
                auto cmd = *p;
                ngClearShortcutFor(cmd);
                ngRegisterShortcut(sc, cmd, false);
            }
        }
    }
}
