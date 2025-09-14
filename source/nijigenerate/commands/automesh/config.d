module nijigenerate.commands.automesh.config;

import nijigenerate.commands.base;
import nijigenerate.viewport.vertex; // ngAutoMeshProcessors
import nijigenerate.viewport.vertex.automesh.meta; // IAutoMeshReflect
import nijigenerate.widgets : incDialog;
import std.algorithm : find, map, filter;
import std.array : array;
import std.string : format;
import i18n;

// Get reflection schema as JSON string for a processor
class AutoMeshGetSchemaCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier")) {
    this(string id) { super(_("Get AutoMesh Schema"), _("Show AutoMesh reflection schema"), id); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        auto p = _resolve(processorId);
        if (!p) return;
        auto r = cast(IAutoMeshReflect)p;
        if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
        auto schema = r.amSchema();
        incDialog(__("Schema"), schema);
    }
}

// Get values for a level (Simple/Advanced)
class AutoMeshGetValuesCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier"), TW!(string, "level", "Config level: Simple/Advanced")) {
    this(string id, string level) { super(_("Get AutoMesh Values"), _("Show AutoMesh config values"), id, level); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        auto p = _resolve(processorId); if (!p) return;
        auto r = cast(IAutoMeshReflect)p; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
        auto vals = r.amValues(level.length ? level : "Simple");
        incDialog(__("Values"), vals);
    }
}

// Apply a preset by name
class AutoMeshSetPresetCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier"), TW!(string, "preset", "Preset name")) {
    this(string id, string preset) { super(_("Set AutoMesh Preset"), _("Apply AutoMesh preset"), id, preset); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        auto p = _resolve(processorId); if (!p) return;
        auto r = cast(IAutoMeshReflect)p; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
        if (!r.amApplyPreset(preset)) incDialog(__("Error"), ("Preset not found: %s").format(preset));
    }
}

// Set values (JSON object id->value/array) for a level
class AutoMeshSetValuesCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier"), TW!(string, "level", "Config level: Simple/Advanced"), TW!(string, "updates", "JSON object of updates")) {
    this(string id, string level, string updates) { super(_("Set AutoMesh Values"), _("Update AutoMesh config values"), id, level, updates); }
    override bool runnable(Context ctx) { return updates.length > 0; }
    override void run(Context ctx) {
        auto p = _resolve(processorId); if (!p) return;
        auto r = cast(IAutoMeshReflect)p; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
        if (!r.amWriteValues(level.length ? level : "Simple", updates)) incDialog(__("Error"), "No values applied");
    }
}

private AutoMeshProcessor _resolve(string id) {
    foreach (pp; ngAutoMeshProcessors()) {
        auto tn = typeid(cast(Object)pp).toString();
        size_t lastDot = 0; bool hasDot = false; foreach (i, ch; tn) if (ch == '.') { lastDot = i; hasDot = true; }
        auto pid = hasDot ? tn[lastDot + 1 .. $] : tn;
        if (pid == id) return pp;
    }
    return null;
}

// Registry
enum AutoMeshConfigCommand {
    AutoMeshGetSchema,
    AutoMeshGetValues,
    AutoMeshSetPreset,
    AutoMeshSetValues,
}

Command[AutoMeshConfigCommand] commands;

void ngInitCommands(T)() if (is(T == AutoMeshConfigCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!AutoMeshConfigCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    // Provide callable instances with default args
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshGetSchema, ""));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshGetValues, "", "Simple"));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshSetPreset, "", ""));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshSetValues, "", "Simple", "{}"));
}
