module nijigenerate.core.shortcut.base;

// Keep this module free of command/UI imports. It provides infrastructure only.
import nijigenerate.commands.base : Command, Context; // base types
import nijigenerate.core.input;            // incShortcut
import nijigenerate.project;               // active/selection state
import bindbc.imgui;
import inmath : vec2u;
import nijilive; // ParameterBinding
import std.conv : to;
import nijigenerate.core.settings; // settings store
import nijigenerate.panels; // incPanels
import nijigenerate.panels.inspector : InspectorPanel; // panel type used by UI
import nijigenerate.panels.inspector.common : TypedInspector; // inspector base

// Action entry representing a shortcut binding to a Command
struct ActionEntry {
    string shortcut;    // e.g. "Ctrl+Shift+P"
    Command command;    // Bound command instance
    bool repeat;        // Whether to allow key repeat
}

// Registry keyed by Command for O(1) lookup and clear
private ActionEntry[Command] gShortcutEntries;
private __gshared bool gShortcutCaptureActive = false; // suppress dispatch during capture

void ngSetShortcutCapture(bool capturing)
{
    gShortcutCaptureActive = capturing;
}

bool ngIsShortcutCapture()
{
    return gShortcutCaptureActive;
}

// Register or overwrite a shortcut (public API)
void ngRegisterShortcut(string shortcut, Command command, bool repeat = false)
{
    if (command is null || shortcut.length == 0) return;

    // Ensure shortcut uniqueness: remove any prior entry with the same shortcut
    import std.array : array;
    auto keys = gShortcutEntries.byKey.array;
    foreach (cmdExisting; keys) {
        auto e = gShortcutEntries[cmdExisting];
        if (e.shortcut == shortcut) {
            gShortcutEntries.remove(cmdExisting);
            break;
        }
    }

    // Clear any old shortcut for this command
    ngClearShortcutFor(command);

    gShortcutEntries[command] = ActionEntry(shortcut, command, repeat);
}

// Remove any existing shortcut(s) bound to the given command instance
void ngClearShortcutFor(Command command)
{
    if (command is null) return;
    gShortcutEntries.remove(command);
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
    if (auto p = cmd in gShortcutEntries)
        return (*p).shortcut;
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

    // Armed parameter(s)
    auto armed = incArmedParameter();
    if (armed !is null)
        ctx.armedParameters = [armed];

    // Selected parameters (if any)
    auto selParams = incSelectedParams();
    if (selParams.length > 0)
        ctx.parameters = selParams;

    // Selected bindings if any (from binding panel context), via provider
    if (gSelectedBindingsProvider !is null) {
        auto bindings = gSelectedBindingsProvider();
        if (bindings.length > 0)
            ctx.bindings = bindings;
    }

    // Current key point for parameter editing (via provider)
    if (gParamPointProvider !is null)
        ctx.keyPoint = gParamPointProvider();

    // Current inspector instances from the InspectorPanel in incPanels
    TypedInspector!Node[] inspectors;
    static InspectorPanel ip = null;
    if (ip is null) {
        foreach (p; incPanels) {
            ip = cast(InspectorPanel)p;
            if (ip !is null)
                break;
        }
    }
    if (ip !is null) {
        if (ip && ip.activeNodeInspectors) {
            auto ins = ip.activeNodeInspectors.getAll();
            foreach (i; ins) inspectors ~= i;
        }
        if (inspectors.length > 0)
            ctx.inspectors = inspectors;
    }
    return ctx;
}

// Handle shortcut inputs each frame: find first matching and execute
void incHandleShortcuts()
{
    if (gShortcutCaptureActive) return;
    auto io = igGetIO();
    foreach (cmd, entry; gShortcutEntries) {
        if (incShortcut(entry.shortcut, entry.repeat)) {
            auto ctx = buildExecutionContext();
            if (entry.command.runnable(ctx))
                entry.command.run(ctx);
            break; // handle one per frame, closest match wins
        }
    }
}

// Public wrapper for building an execution context for commands outside this module
Context ngBuildExecutionContext()
{
    return buildExecutionContext();
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
    string[string] obj; // string map
    foreach (cmd, entry; gShortcutEntries) {
        auto id = _commandIdFor(entry.command);
        if (id.length)
            obj[id] = entry.shortcut;
    }
    incSettingsSet("Shortcuts", obj);
}

// Load shortcuts from settings and register them, overriding existing bindings
void ngLoadShortcutsFromSettings()
{
    if (!incSettingsCanGet("Shortcuts")) return;
    auto m = incSettingsGet!(string[string])("Shortcuts");
    if (m.length == 0) return;

    auto all = _buildCommandIdMap();
    foreach (key, sc; m) {
        if (auto p = key in all) {
            if (sc.length) {
                auto cmd = *p;
                ngClearShortcutFor(cmd);
                ngRegisterShortcut(sc, cmd, false);
            }
        }
    }
}
