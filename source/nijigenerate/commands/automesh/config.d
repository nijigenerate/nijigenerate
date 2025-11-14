module nijigenerate.commands.automesh.config;

import nijigenerate.commands.base;
import nijigenerate.viewport.vertex; // ngAutoMeshProcessors, ngActiveAutoMeshProcessor
import nijigenerate.viewport.vertex.automesh : AutoMeshProcessorTypes; // compile-time types
import nijigenerate.viewport.vertex.automesh.meta : AMProcInfo, AMParam, AMEnum; // UDA info
import nijigenerate.commands.automesh.dynamic : ensureApplyAutoMeshCommand; // reuse robust apply
import nijigenerate.viewport.vertex.automesh.meta; // IAutoMeshReflect
import nijigenerate.widgets : incDialog;
import std.algorithm : find, map, filter;
import std.array : array;
import std.string : format;
import i18n;
import bindbc.imgui; // optional UI list rendering

// Compile-time presence check for initializer
static if (__traits(compiles, { void _ct_probe(){ ngInitCommands!(AutoMeshTypedCommand)(); } })) {
    //    pragma(msg, "[CT] automesh.config: ngInitCommands!(AutoMeshTypedCommand) present");
} else
    pragma(msg, "[CT] automesh.config: ngInitCommands!(AutoMeshTypedCommand) MISSING");

// ===== Typed per-processor enum and map (for MCP tool discovery) =====
private string _sanitizeId(string s) {
    import std.ascii : isAlphaNum;
    string r;
    foreach (ch; s) r ~= (isAlphaNum(ch) ? ch : '_');
    return r;
}
private string _genTypedEnumMembers()() {
    string s;
    static foreach (i, PT; AutoMeshProcessorTypes) {{
        enum base_ = _sanitizeId(__traits(identifier, PT));
        s ~= "AutoMeshSetSimple_" ~ base_ ~ ",";
        s ~= "AutoMeshSetAdvanced_" ~ base_ ~ ",";
        s ~= "AutoMeshSetPreset_" ~ base_ ~ ",";
    }}
    return s;
}
mixin("enum AutoMeshTypedCommand { " ~ _genTypedEnumMembers() ~ "}");
//pragma(msg, "[CT] AutoMeshTypedCommand members: " ~ _genTypedEnumMembers());

Command[AutoMeshTypedCommand] autoMeshTypedCommands;

// Get reflection schema as JSON string for a processor
class AutoMeshGetSchemaCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier")) {
    this(string id) { super(_("Get AutoMesh Schema"), _("Show AutoMesh reflection schema"), id); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        auto p = _resolve(processorId);
        if (!p) return;
        auto r = cast(IAutoMeshReflect)p;
        if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
        auto schema = r.schema();
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
        auto vals = r.values(level.length ? level : "Simple");
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
        if (!r.applyPreset(preset)) incDialog(__("Error"), ("Preset not found: %s").format(preset));
    }
}

// Set values (JSON object id->value/array) for a level
class AutoMeshSetValuesCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier"), TW!(string, "level", "Config level: Simple/Advanced"), TW!(string, "updates", "JSON object of updates")) {
    this(string id, string level, string updates) { super(_("Set AutoMesh Values"), _("Update AutoMesh config values"), id, level, updates); }
    override bool runnable(Context ctx) { return updates.length > 0; }
    override void run(Context ctx) {
        auto p = _resolve(processorId); if (!p) return;
        auto r = cast(IAutoMeshReflect)p; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
        if (!r.writeValues(level.length ? level : "Simple", updates)) incDialog(__("Error"), "No values applied");
    }
}

private AutoMeshProcessor _resolve(string id) {
    foreach (pp; ngAutoMeshProcessors()) {
        if (pp.procId() == id) return pp;
    }
    return null;
}

// Registry
enum AutoMeshConfigCommand {
    AutoMeshListProcessors,
    AutoMeshGetActive,
    AutoMeshSetActive,
    AutoMeshApplyActive,
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
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshListProcessors));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshGetActive));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshSetActive, ""));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshApplyActive));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshGetSchema, ""));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshGetValues, "", "Simple"));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshSetPreset, "", ""));
    mixin(registerCommand!(AutoMeshConfigCommand.AutoMeshSetValues, "", "Simple", "{}"));
}

// List available AutoMesh processors with ids, icons and reflectable flag
class AutoMeshListProcessorsCommand : ExCommand!() {
    this() { super(_("List AutoMesh Processors"), _("Show available AutoMesh processors")); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        import std.json : JSONValue, JSONType;
        JSONValue arr = JSONValue(JSONType.array);
        static foreach (i, PT; AutoMeshProcessorTypes) {{
            JSONValue obj_;
            obj_["id"] = AMProcInfo!(PT).id;
            obj_["name"] = AMProcInfo!(PT).name;
            obj_["order"] = AMProcInfo!(PT).order;
            auto inst__ = new PT();
            obj_["icon"] = inst__.icon();
            obj_["reflectable"] = (cast(IAutoMeshReflect)inst__) !is null;
            arr.array ~= obj_;
        }}
        incDialog(__("AutoMesh Processors"), arr.toString());
    }
}

// ===== Per-processor reflection commands (compile-time generated) =====

// Resolve runtime processor instance by type
private AutoMeshProcessor _resolveInstance(alias PT)() {
    foreach (pp; ngAutoMeshProcessors()) if (cast(PT)pp) return pp;
    return null;
}

// GetConfig per-processor
template GetAutoMeshConfigPT(alias PT)
{
    class GetAutoMeshConfigPT : ExCommand!(TW!(string, "level", "Config level: Simple/Advanced"))
    {
        this(string level) {
            super("Get Config (" ~ AMProcInfo!(PT).name ~ ")", "Get AutoMesh config (per processor)", level);
        }
        override bool runnable(Context ctx) { return true; }
        override void run(Context ctx) {
            auto inst = _resolveInstance!PT(); if (!inst) return;
            auto r = cast(IAutoMeshReflect)inst; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
            auto vals = r.values(level.length ? level : "Simple");
            incDialog(__("Values"), vals);
        }
    }
}

// SetConfig per-processor
template SetAutoMeshConfigPT(alias PT)
{
    class SetAutoMeshConfigPT : ExCommand!(TW!(string, "level", "Config level: Simple/Advanced"), TW!(string, "updates", "JSON object of updates"))
    {
        this(string level, string updates) {
            super("Set Config (" ~ AMProcInfo!(PT).name ~ ")", "Set AutoMesh config (per processor)", level, updates);
        }
        override bool runnable(Context ctx) { return updates.length > 0; }
        override void run(Context ctx) {
            auto inst = _resolveInstance!PT(); if (!inst) return;
            auto r = cast(IAutoMeshReflect)inst; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
            if (!r.writeValues(level.length ? level : "Simple", updates)) incDialog(__("Error"), "No values applied");
        }
    }
}

// Maps and initializers for per-processor Get/Set commands
struct AutoMeshGetConfigKey { string id; }
struct AutoMeshSetConfigKey { string id; }
Command[AutoMeshGetConfigKey] autoMeshGetConfigCommands;
Command[AutoMeshSetConfigKey] autoMeshSetConfigCommands;

Command ensureGetAutoMeshConfigCommand(string id)
{
    AutoMeshGetConfigKey key = AutoMeshGetConfigKey(id);
    if (auto p = key in autoMeshGetConfigCommands) return *p;
    static foreach (i, PT; AutoMeshProcessorTypes) {{
        enum pid_ = AMProcInfo!(PT).id;
        static if (pid_.length) {
            if (pid_ == id) { autoMeshGetConfigCommands[key] = cast(Command) new GetAutoMeshConfigPT!PT("Simple"); return autoMeshGetConfigCommands[key]; }
        }
    }}
    return null;
}
Command ensureSetAutoMeshConfigCommand(string id)
{
    AutoMeshSetConfigKey key = AutoMeshSetConfigKey(id);
    if (auto p = key in autoMeshSetConfigCommands) return *p;
    static foreach (i, PT; AutoMeshProcessorTypes) {{
        enum pid_ = AMProcInfo!(PT).id;
        static if (pid_.length) {
            if (pid_ == id) { autoMeshSetConfigCommands[key] = cast(Command) new SetAutoMeshConfigPT!PT("Simple", "{}"); return autoMeshSetConfigCommands[key]; }
        }
    }}
    return null;
}

void ngInitCommands(T)() if (is(T == AutoMeshGetConfigKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {
        enum pid = AMProcInfo!(PT).id;
        AutoMeshGetConfigKey key = AutoMeshGetConfigKey(pid);
        autoMeshGetConfigCommands[key] = cast(Command) new GetAutoMeshConfigPT!PT("Simple");
    }
}
void ngInitCommands(T)() if (is(T == AutoMeshSetConfigKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {
        enum pid = AMProcInfo!(PT).id;
        AutoMeshSetConfigKey key = AutoMeshSetConfigKey(pid);
        autoMeshSetConfigCommands[key] = cast(Command) new SetAutoMeshConfigPT!PT("Simple", "{}");
    }
}

// ===== Strongly-typed level Set commands per processor (Simple/Advanced), and preset =====

// Helpers to generate TW parameter list and setters for a level
private string _genTwListForLevel(alias PT, AutoMeshLevel levelV)()
{
    string list;
    static foreach (mname; __traits(allMembers, PT)) {{
        static if (__traits(compiles, __traits(getMember, PT, mname))) {{
            AMParam p; bool hasParam = false; AMEnum e; bool hasEnum = false;
            enum bool _isFunctionMember = __traits(compiles, __traits(getOverloads, PT, mname));
            static if (!_isFunctionMember) {
                alias Member = __traits(getMember, PT, mname);
                foreach (attr; __traits(getAttributes, Member)) {
                    static if (is(typeof(attr) == AMParam)) { p = attr; hasParam = true; }
                    static if (is(typeof(attr) == AMEnum)) { e = attr; hasEnum = true; }
                }
            }
            if (hasParam && p.level == levelV) {
                static if (is(typeof(__traits(getMember, PT, mname)) == float)) {
                    list ~= `TW!(float, "` ~ p.id ~ `", "` ~ p.label ~ `"),`;
                } else static if (is(typeof(__traits(getMember, PT, mname)) == float[])) {
                    list ~= `TW!(float[], "` ~ p.id ~ `", "` ~ p.label ~ ` (array)"),`;
                } else {
                    if (hasEnum) static if (is(typeof(__traits(getMember, PT, mname)) == string)) {
                        list ~= `TW!(int, "` ~ p.id ~ `", "` ~ p.label ~ ` (index)"),`;
                    }
                }
            }
        }}
    }}
    if (list.length && list[$-1] == ',') list = list[0 .. $-1];
    return list.length ? list : "";
}

private string _genSettersForLevel(alias PT, AutoMeshLevel levelV)()
{
    string code;
    static foreach (mname; __traits(allMembers, PT)) {{
        static if (__traits(compiles, __traits(getMember, PT, mname))) {{
            AMParam p; bool hasParam = false; AMEnum e; bool hasEnum = false;
            enum bool _isFunctionMember = __traits(compiles, __traits(getOverloads, PT, mname));
            static if (!_isFunctionMember) {
                alias Member = __traits(getMember, PT, mname);
                foreach (attr; __traits(getAttributes, Member)) {
                    static if (is(typeof(attr) == AMParam)) { p = attr; hasParam = true; }
                    static if (is(typeof(attr) == AMEnum)) { e = attr; hasEnum = true; }
                }
            }
            if (hasParam && p.level == levelV) {
                static if (is(typeof(__traits(getMember, PT, mname)) == float)) {
                    import std.conv : to;
                    string minClamp, maxClamp;
                    if (p.min == p.min) { auto minStr = to!string(p.min); minClamp = ` if (v < ` ~ minStr ~ `) v = ` ~ minStr ~ `;`; }
                    if (p.max == p.max) { auto maxStr = to!string(p.max); maxClamp = ` if (v > ` ~ maxStr ~ `) v = ` ~ maxStr ~ `;`; }
                    code ~= `{
                        float v = ` ~ p.id ~ `;` ~ minClamp ~ maxClamp ~ `
                        (cast(PT)inst).` ~ mname ~ ` = v;
                        static if (__traits(hasMember, PT, "ngPostParamWrite")) (cast(PT)inst).ngPostParamWrite("` ~ p.id ~ `");
                    }
                    `;
                } else static if (is(typeof(__traits(getMember, PT, mname)) == float[])) {
                    code ~= `{
                        (cast(PT)inst).` ~ mname ~ ` = ` ~ p.id ~ `;
                        static if (__traits(hasMember, PT, "ngPostParamWrite")) (cast(PT)inst).ngPostParamWrite("` ~ p.id ~ `");
                    }
                    `;
                } else {
                    if (hasEnum) static if (is(typeof(__traits(getMember, PT, mname)) == string)) {
                        string cases;
                        foreach (i, v; e.values) cases ~= `case ` ~ i.stringof ~ `: val = "` ~ v ~ `"; break;`;
                        code ~= `{
                            int idx = ` ~ p.id ~ `; string val; switch(idx){` ~ cases ~ `default: break;} (cast(PT)inst).` ~ mname ~ ` = val;
                            static if (__traits(hasMember, PT, "ngPostParamWrite")) (cast(PT)inst).ngPostParamWrite("` ~ p.id ~ `");
                        }
                        `;
                    }
                }
            }
        }}
    }}
    return code;
}

// Simple-level setter per PT
template AutoMeshSetSimpleConfigCommand(alias PT)
{
    enum string _tw = _genTwListForLevel!(PT, AutoMeshLevel.Simple)();
    enum string _cls = "AutoMeshSetSimple_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command";
    mixin(`class ` ~ _cls ~ ` : ExCommand!(` ~ (_tw.length ? _tw : "") ~ `) {
        this() { super("Set Simple (" ~ AMProcInfo!(PT).name ~ ")", "Set simple config"); }
        override bool runnable(Context ctx) { return true; }
        override void run(Context ctx) { auto inst = _resolveInstance!PT(); if (!inst) return; ` ~ _genSettersForLevel!(PT, AutoMeshLevel.Simple)() ~ ` }
    }`);
}

// Advanced-level setter per PT
template AutoMeshSetAdvancedConfigCommand(alias PT)
{
    enum string _tw = _genTwListForLevel!(PT, AutoMeshLevel.Advanced)();
    enum string _cls = "AutoMeshSetAdvanced_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command";
    mixin(`class ` ~ _cls ~ ` : ExCommand!(` ~ (_tw.length ? _tw : "") ~ `) {
        this() { super("Set Advanced (" ~ AMProcInfo!(PT).name ~ ")", "Set advanced config"); }
        override bool runnable(Context ctx) { return true; }
        override void run(Context ctx) { auto inst = _resolveInstance!PT(); if (!inst) return; ` ~ _genSettersForLevel!(PT, AutoMeshLevel.Advanced)() ~ ` }
    }`);
}

// Preset setter per PT
template AutoMeshSetPresetTypedCommand(alias PT)
{
    enum string _cls = "AutoMeshSetPreset_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command";
    mixin(`class ` ~ _cls ~ ` : ExCommand!(TW!(string, "preset", "Preset name"))
    {
        this() { super("Set Preset (" ~ AMProcInfo!(PT).name ~ ")", "Apply preset"); }
        override bool runnable(Context ctx) { return true; }
        override void run(Context ctx) {
            auto inst = _resolveInstance!PT(); if (!inst) return;
            auto r = cast(IAutoMeshReflect)inst; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return; }
            if (!r.applyPreset(preset)) incDialog(__("Error"), ("Preset not found: %s").format(preset));
        }
    }`);
}

// Emit typed command classes at module scope so they are globally resolvable
static foreach (PT; AutoMeshProcessorTypes) {
    mixin AutoMeshSetSimpleConfigCommand!PT;
    mixin AutoMeshSetAdvancedConfigCommand!PT;
    mixin AutoMeshSetPresetTypedCommand!PT;
}

// Maps for level/preset setters
struct AutoMeshSetSimpleKey { string id; }
struct AutoMeshSetAdvancedKey { string id; }
struct AutoMeshSetPresetKey { string id; }
Command[AutoMeshSetSimpleKey] autoMeshSetSimpleCommands;
Command[AutoMeshSetAdvancedKey] autoMeshSetAdvancedCommands;
Command[AutoMeshSetPresetKey] autoMeshSetPresetCommands;

void ngInitCommands(T)() if (is(T == AutoMeshSetSimpleKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        AutoMeshSetSimpleKey key = AutoMeshSetSimpleKey(pid);
        autoMeshSetSimpleCommands[key] = cast(Command) new AutoMeshSetSimpleConfigCommand!PT();
    }}
}
void ngInitCommands(T)() if (is(T == AutoMeshSetAdvancedKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        AutoMeshSetAdvancedKey key = AutoMeshSetAdvancedKey(pid);
        autoMeshSetAdvancedCommands[key] = cast(Command) new AutoMeshSetAdvancedConfigCommand!PT();
    }}
}
void ngInitCommands(T)() if (is(T == AutoMeshSetPresetKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        AutoMeshSetPresetKey key = AutoMeshSetPresetKey(pid);
        autoMeshSetPresetCommands[key] = cast(Command) new AutoMeshSetPresetTypedCommand!PT("");
    }}
}

// Map enum members to generated classes by convention: <EnumName>Command
void ngInitCommands(T)() if (is(T == AutoMeshTypedCommand))
{
    // Compile-time: dump per-processor reflected params for verification
    template _ctDumpParams(alias PT) {
        /*
        enum string _name = AMProcInfo!(PT).name;
        enum string _dump = ({
            string s;
            static foreach (mname; __traits(allMembers, PT)) {
                static if (__traits(compiles, __traits(getMember, PT, mname))) {
                    // Only inspect field-like members (float, string, float[])
                    static if (is(typeof(__traits(getMember, PT, mname)) == float) || is(typeof(__traits(getMember, PT, mname)) == string) || is(typeof(__traits(getMember, PT, mname)) == float[])) {
                        static foreach (attr; __traits(getAttributes, __traits(getMember, PT, mname))) {
                            static if (is(typeof(attr) == AMParam)) {
                                s ~= "[CT][AutoMeshParams] " ~ _name ~ "." ~ mname ~ ": id=" ~ attr.id ~ ", level=" ~ attr.level.stringof;
                                static if (is(typeof(__traits(getMember, PT, mname)) == float)) s ~= ", type=float\n";
                                else static if (is(typeof(__traits(getMember, PT, mname)) == string)) s ~= ", type=string\n";
                                else static if (is(typeof(__traits(getMember, PT, mname)) == float[])) s ~= ", type=float[]\n";
                            }
                        }
                    }
                }
            }
            return s;
        })();
        pragma(msg, _dump);
        */
    }
    static foreach (PT; AutoMeshProcessorTypes) { mixin _ctDumpParams!PT; }

    // Register all enum members to their corresponding classes by name
    import std.stdio : writefln;
    size_t before = 0; foreach (_k, _v; autoMeshTypedCommands) ++before;
    static foreach (n; __traits(allMembers, AutoMeshTypedCommand)) {{
        static if (n != "init" && n != "min" && n != "max" && n != "stringof") {
            static if (__traits(compiles, mixin("AutoMeshTypedCommand."~n))) {
//                pragma(msg, "[CT] Register typed command class for enum: " ~ n);
                alias KS = mixin("AutoMeshTypedCommand."~n);
                alias C  = mixin(n ~ "Command");
                autoMeshTypedCommands[KS] = cast(Command) new C();
            }
        }
    }}
    size_t after = 0; foreach (_k, _v; autoMeshTypedCommands) ++after;
    writefln("[CMD] AutoMeshTypedCommand init: before=%s after=%s", before, after);
}
// Get currently active AutoMesh processor
class AutoMeshGetActiveCommand : ExCommand!() {
    this() { super(_("Get Active AutoMesh"), _("Show active AutoMesh processor")); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        import std.json : JSONValue, JSONType;
        auto p = ngActiveAutoMeshProcessor();
        JSONValue o; o["id"] = p.procId(); o["name"] = p.displayName(); o["icon"] = p.icon(); o["reflectable"] = (cast(IAutoMeshReflect)p) !is null;
        incDialog(__("Active AutoMesh"), o.toString());
    }
}

// Set active AutoMesh processor by id
class AutoMeshSetActiveCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier")) {
    this(string id) { super(_("Set Active AutoMesh"), _("Activate AutoMesh processor"), id); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        foreach (pp; ngAutoMeshProcessors()) if (pp.procId() == processorId) { ngActiveAutoMeshProcessor(pp); return; }
        incDialog(__("Error"), ("Processor not found: %s").format(processorId));
    }
}

// Apply active AutoMesh to current context/selection (reuses robust Apply command)
class AutoMeshApplyActiveCommand : ExCommand!() {
    this() { super(_("Apply Active AutoMesh"), _("Apply active AutoMesh to targets")); }
    override bool runnable(Context ctx) { return true; }
    override void run(Context ctx) {
        auto p = ngActiveAutoMeshProcessor();
        auto cmd = ensureApplyAutoMeshCommand(p.procId());
        cmd.run(ctx);
    }
}
