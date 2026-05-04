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
import std.stdio : writefln;

version(CMD_LOG) private void cmdLog(T...)(T args) { writefln(args); }
else             private void cmdLog(T...)(T args) {}

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
@McpHidden
@GuiDialogOutput
class AutoMeshGetSchemaCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier")) {
    this(string id) { super(_("Get AutoMesh Schema"), _("Show AutoMesh reflection schema"), id); }
    override bool runnable(Context ctx) { return true; }
    override CommandResult run(Context ctx) {
        auto p = _resolve(processorId);
        if (!p) return CommandResult(false, "Processor not found");
        auto r = cast(IAutoMeshReflect)p;
        if (!r) { incDialog(__("Error"), "Processor not reflectable"); return CommandResult(false, "Not reflectable"); }
        auto schema = r.schema();
        incDialog(__("Schema"), schema);
        return CommandResult(true);
    }
}

// Get values for a level (Simple/Advanced)
@McpHidden
@GuiDialogOutput
class AutoMeshGetValuesCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier"), TW!(string, "level", "Config level: Simple/Advanced")) {
    this(string id, string level) { super(_("Get AutoMesh Values"), _("Show AutoMesh config values"), id, level); }
    override bool runnable(Context ctx) { return true; }
    override CommandResult run(Context ctx) {
        auto p = _resolve(processorId); if (!p) return CommandResult(false, "Processor not found");
        auto r = cast(IAutoMeshReflect)p; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return CommandResult(false, "Not reflectable"); }
        auto vals = r.values(level.length ? level : "Simple");
        incDialog(__("Values"), vals);
        return CommandResult(true);
    }
}

// Apply a preset by name
@EffectConfigEdit
class AutoMeshSetPresetCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier"), TW!(string, "preset", "Preset name")) {
    this(string id, string preset) { super(_("Set AutoMesh Preset"), _("Apply AutoMesh preset"), id, preset); }
    override bool runnable(Context ctx) { return true; }
    override CommandResult run(Context ctx) {
        auto p = _resolve(processorId); if (!p) return CommandResult(false, "Processor not found");
        auto r = cast(IAutoMeshReflect)p; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return CommandResult(false, "Not reflectable"); }
        if (!r.applyPreset(preset)) { incDialog(__("Error"), ("Preset not found: %s").format(preset)); return CommandResult(false, "Preset not found"); }
        return CommandResult(true);
    }
}

// Set values (JSON object id->value/array) for a level
@EffectConfigEdit
class AutoMeshSetValuesCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier"), TW!(string, "level", "Config level: Simple/Advanced"), TW!(string, "updates", "JSON object of updates")) {
    this(string id, string level, string updates) { super(_("Set AutoMesh Values"), _("Update AutoMesh config values"), id, level, updates); }
    override bool runnable(Context ctx) { return updates.length > 0; }
    override CommandResult run(Context ctx) {
        auto p = _resolve(processorId); if (!p) return CommandResult(false, "Processor not found");
        auto r = cast(IAutoMeshReflect)p; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return CommandResult(false, "Not reflectable"); }
        if (!r.writeValues(level.length ? level : "Simple", updates)) { incDialog(__("Error"), "No values applied"); return CommandResult(false, "No values applied"); }
        return CommandResult(true);
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
@McpHidden
@GuiDialogOutput
class AutoMeshListProcessorsCommand : ExCommand!() {
    this() { super(_("List AutoMesh Processors"), _("Show available AutoMesh processors")); }
    override bool runnable(Context ctx) { return true; }
    override CommandResult run(Context ctx) {
        import std.json : JSONValue, JSONType;
        JSONValue arr = JSONValue.emptyArray;
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
        return CommandResult(true);
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
@McpHidden
    @GuiDialogOutput
    class GetAutoMeshConfigPT : ExCommand!(TW!(string, "level", "Config level: Simple/Advanced"))
    {
        this(string level) {
            super("Get Config (" ~ AMProcInfo!(PT).name ~ ")", "Get AutoMesh config (per processor)", level);
        }
        override bool runnable(Context ctx) { return true; }
        override CommandResult run(Context ctx) {
            auto inst = _resolveInstance!PT(); if (!inst) return CommandResult(false, "Processor not found");
            auto r = cast(IAutoMeshReflect)inst; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return CommandResult(false, "Not reflectable"); }
            auto vals = r.values(level.length ? level : "Simple");
            incDialog(__("Values"), vals);
            return CommandResult(true);
        }
    }
}

// SetConfig per-processor
template SetAutoMeshConfigPT(alias PT)
{
    @EffectConfigEdit
    class SetAutoMeshConfigPT : ExCommand!(TW!(string, "level", "Config level: Simple/Advanced"), TW!(string, "updates", "JSON object of updates"))
    {
        this(string level, string updates) {
            super("Set Config (" ~ AMProcInfo!(PT).name ~ ")", "Set AutoMesh config (per processor)", level, updates);
        }
        override bool runnable(Context ctx) { return updates.length > 0; }
        override CommandResult run(Context ctx) {
            auto inst = _resolveInstance!PT(); if (!inst) return CommandResult(false, "Processor not found");
            auto r = cast(IAutoMeshReflect)inst; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return CommandResult(false, "Not reflectable"); }
            if (!r.writeValues(level.length ? level : "Simple", updates)) { incDialog(__("Error"), "No values applied"); return CommandResult(false, "No values applied"); }
            return CommandResult(true);
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
            if (pid_ == id) { auto cmd = new GetAutoMeshConfigPT!PT("Simple"); ngRegisterCommandMeta(cmd); autoMeshGetConfigCommands[key] = cmd; return autoMeshGetConfigCommands[key]; }
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
            if (pid_ == id) { auto cmd = new SetAutoMeshConfigPT!PT("Simple", "{}"); ngRegisterCommandMeta(cmd); autoMeshSetConfigCommands[key] = cmd; return autoMeshSetConfigCommands[key]; }
        }
    }}
    return null;
}

void ngInitCommands(T)() if (is(T == AutoMeshGetConfigKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        AutoMeshGetConfigKey key = AutoMeshGetConfigKey(pid);
        auto cmd = new GetAutoMeshConfigPT!PT("Simple");
        ngRegisterCommandMeta(cmd);
        autoMeshGetConfigCommands[key] = cmd;
    }}
}
void ngInitCommands(T)() if (is(T == AutoMeshSetConfigKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        AutoMeshSetConfigKey key = AutoMeshSetConfigKey(pid);
        auto cmd = new SetAutoMeshConfigPT!PT("Simple", "{}");
        ngRegisterCommandMeta(cmd);
        autoMeshSetConfigCommands[key] = cmd;
    }}
}

// ===== Strongly-typed level Set commands per processor (Simple/Advanced), and preset =====

// Helpers to generate TW parameter list and setters for a level
private string _genTwListForLevel(alias PT, AutoMeshLevel levelV)()
{
    string list;
    static foreach (mname; __traits(allMembers, PT)) {{
        static if (__traits(compiles, __traits(getMember, PT, mname))) {{
            AMParam p; bool hasParam = false; AMEnum e; bool hasEnum = false;
            static if (is(typeof(__traits(getMember, PT, mname)) == float)
                    || is(typeof(__traits(getMember, PT, mname)) == float[])
                    || is(typeof(__traits(getMember, PT, mname)) == string)) {
                alias Member = __traits(getMember, PT, mname);
                foreach (attr; __traits(getAttributes, Member)) {
                    static if (is(typeof(attr) == AMParam)) { p = attr; hasParam = true; }
                    static if (is(typeof(attr) == AMEnum)) { e = attr; hasEnum = true; }
                }

                if (hasParam && p.level == levelV) {
                    static if (is(typeof(__traits(getMember, PT, mname)) == float)) {
                        list ~= `TW!(float, "` ~ p.id ~ `", "` ~ p.label ~ `"),`;
                    } else static if (is(typeof(__traits(getMember, PT, mname)) == float[])) {
                        list ~= `TW!(float[], "` ~ p.id ~ `", "` ~ p.label ~ ` (array)"),`;
                    } else static if (is(typeof(__traits(getMember, PT, mname)) == string)) {
                        if (hasEnum) {
                        string choices;
                        foreach (i, v; e.values) {
                            if (i > 0) choices ~= "|";
                            choices ~= v;
                        }
                        list ~= `TW!(string, "` ~ p.id ~ `", "` ~ p.label ~ ` (` ~ choices ~ `)"),`;
                        }
                    }
                }
            }
        }}
    }}
    if (list.length && list[$-1] == ',') list = list[0 .. $-1];
    return list.length ? list : "";
}

private string _genParamSummaryForLevel(alias PT, AutoMeshLevel levelV)()
{
    string summary;
    static foreach (mname; __traits(allMembers, PT)) {{
        static if (__traits(compiles, __traits(getMember, PT, mname))) {{
            AMParam p; bool hasParam = false; AMEnum e; bool hasEnum = false;
            static if (is(typeof(__traits(getMember, PT, mname)) == float)
                    || is(typeof(__traits(getMember, PT, mname)) == float[])
                    || is(typeof(__traits(getMember, PT, mname)) == string)) {
                alias Member = __traits(getMember, PT, mname);
                foreach (attr; __traits(getAttributes, Member)) {
                    static if (is(typeof(attr) == AMParam)) { p = attr; hasParam = true; }
                    static if (is(typeof(attr) == AMEnum)) { e = attr; hasEnum = true; }
                }

                if (hasParam && p.level == levelV) {
                    if (summary.length) summary ~= "; ";
                    summary ~= p.id ~ "=" ~ p.label;
                    static if (is(typeof(__traits(getMember, PT, mname)) == float[])) {
                        summary ~= " array";
                    } else static if (is(typeof(__traits(getMember, PT, mname)) == string)) {
                        if (hasEnum) {
                            string choices;
                            foreach (i, v; e.values) {
                                if (i > 0) choices ~= "|";
                                choices ~= v;
                            }
                            if (choices.length) summary ~= " (" ~ choices ~ ")";
                        }
                    }
                    if (p.desc.length) summary ~= " - " ~ p.desc;
                }
            }
        }}
    }}
    return summary.length ? summary : "No configurable parameters for this level.";
}

private string _levelName(AutoMeshLevel levelV) {
    final switch (levelV) {
        case AutoMeshLevel.Preset: return "Preset";
        case AutoMeshLevel.Simple: return "Simple";
        case AutoMeshLevel.Advanced: return "Advanced";
    }
}

private string _genSetCommandDescription(alias PT, AutoMeshLevel levelV)()
{
    return "Set " ~ _levelName(levelV) ~ " AutoMesh configuration for processor '" ~
        AMProcInfo!(PT).name ~ "' (id: " ~ AMProcInfo!(PT).id ~ "). Parameters: " ~
        _genParamSummaryForLevel!(PT, levelV)();
}

private string _genSettersForLevel(alias PT, AutoMeshLevel levelV)()
{
    string code;
    static foreach (mname; __traits(allMembers, PT)) {{
        static if (__traits(compiles, __traits(getMember, PT, mname))) {{
            AMParam p; bool hasParam = false; AMEnum e; bool hasEnum = false;
            static if (is(typeof(__traits(getMember, PT, mname)) == float)
                    || is(typeof(__traits(getMember, PT, mname)) == float[])
                    || is(typeof(__traits(getMember, PT, mname)) == string)) {
                alias Member = __traits(getMember, PT, mname);
                foreach (attr; __traits(getAttributes, Member)) {
                    static if (is(typeof(attr) == AMParam)) { p = attr; hasParam = true; }
                    static if (is(typeof(attr) == AMEnum)) { e = attr; hasEnum = true; }
                }

                if (hasParam && p.level == levelV) {
                    static if (is(typeof(__traits(getMember, PT, mname)) == float)) {
                        import std.conv : to;
                        string minClamp, maxClamp;
                        if (p.min == p.min) { auto minStr = to!string(p.min); minClamp = ` if (v < ` ~ minStr ~ `) v = ` ~ minStr ~ `;`; }
                        if (p.max == p.max) { auto maxStr = to!string(p.max); maxClamp = ` if (v > ` ~ maxStr ~ `) v = ` ~ maxStr ~ `;`; }
                        code ~= `{
                            float v = ` ~ p.id ~ `;
                            if (v != v) goto skip_` ~ p.id ~ `;` ~ minClamp ~ maxClamp ~ `
                            (cast(PT)inst).` ~ mname ~ ` = v;
                            static if (__traits(hasMember, PT, "ngPostParamWrite")) (cast(PT)inst).ngPostParamWrite("` ~ p.id ~ `");
                            skip_` ~ p.id ~ `:
                        }
                        `;
                    } else static if (is(typeof(__traits(getMember, PT, mname)) == float[])) {
                        code ~= `{
                            if (` ~ p.id ~ ` !is null) {
                                (cast(PT)inst).` ~ mname ~ ` = ` ~ p.id ~ `;
                                static if (__traits(hasMember, PT, "ngPostParamWrite")) (cast(PT)inst).ngPostParamWrite("` ~ p.id ~ `");
                            }
                        }
                        `;
                    } else static if (is(typeof(__traits(getMember, PT, mname)) == string)) {
                        if (hasEnum) {
                            string cases;
                            foreach (v; e.values) cases ~= `case "` ~ v ~ `": val = "` ~ v ~ `"; break;`;
                            code ~= `{
                                string choice = ` ~ p.id ~ `; string val; switch(choice){` ~ cases ~ `default: break;} if (val.length) (cast(PT)inst).` ~ mname ~ ` = val;
                                static if (__traits(hasMember, PT, "ngPostParamWrite")) (cast(PT)inst).ngPostParamWrite("` ~ p.id ~ `");
                            }
                            `;
                        }
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
    enum string _desc = _genSetCommandDescription!(PT, AutoMeshLevel.Simple)();
    mixin(`@EffectConfigEdit class ` ~ _cls ~ ` : ExCommand!(` ~ (_tw.length ? _tw : "") ~ `) {
        this() { super("Set Simple (" ~ AMProcInfo!(PT).name ~ ")", "` ~ _desc ~ `"); }
        override bool runnable(Context ctx) { return true; }
        override CommandResult run(Context ctx) { auto inst = _resolveInstance!PT(); if (!inst) return CommandResult(false, "Processor not found"); ` ~ _genSettersForLevel!(PT, AutoMeshLevel.Simple)() ~ ` return CommandResult(true); }
    }`);
}

// Advanced-level setter per PT
template AutoMeshSetAdvancedConfigCommand(alias PT)
{
    enum string _tw = _genTwListForLevel!(PT, AutoMeshLevel.Advanced)();
    enum string _cls = "AutoMeshSetAdvanced_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command";
    enum string _desc = _genSetCommandDescription!(PT, AutoMeshLevel.Advanced)();
    mixin(`@EffectConfigEdit class ` ~ _cls ~ ` : ExCommand!(` ~ (_tw.length ? _tw : "") ~ `) {
        this() { super("Set Advanced (" ~ AMProcInfo!(PT).name ~ ")", "` ~ _desc ~ `"); }
        override bool runnable(Context ctx) { return true; }
        override CommandResult run(Context ctx) { auto inst = _resolveInstance!PT(); if (!inst) return CommandResult(false, "Processor not found"); ` ~ _genSettersForLevel!(PT, AutoMeshLevel.Advanced)() ~ ` return CommandResult(true); }
    }`);
}

// Preset setter per PT
template AutoMeshSetPresetTypedCommand(alias PT)
{
    enum string _cls = "AutoMeshSetPreset_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command";
    mixin(`@EffectConfigEdit class ` ~ _cls ~ ` : ExCommand!(TW!(string, "preset", "Preset name"))
    {
        this() { super("Set Preset (" ~ AMProcInfo!(PT).name ~ ")", "Apply preset"); }
        override bool runnable(Context ctx) { return true; }
        override CommandResult run(Context ctx) {
            auto inst = _resolveInstance!PT(); if (!inst) return CommandResult(false, "Processor not found");
            auto r = cast(IAutoMeshReflect)inst; if (!r) { incDialog(__("Error"), "Processor not reflectable"); return CommandResult(false, "Not reflectable"); }
            if (!r.applyPreset(preset)) { incDialog(__("Error"), ("Preset not found: %s").format(preset)); return CommandResult(false, "Preset not found"); }
            return CommandResult(true);
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
        alias C = mixin("AutoMeshSetSimple_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command");
        auto cmd = new C();
        ngRegisterCommandMeta(cmd);
        autoMeshSetSimpleCommands[key] = cmd;
    }}
}
void ngInitCommands(T)() if (is(T == AutoMeshSetAdvancedKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        AutoMeshSetAdvancedKey key = AutoMeshSetAdvancedKey(pid);
        alias C = mixin("AutoMeshSetAdvanced_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command");
        auto cmd = new C();
        ngRegisterCommandMeta(cmd);
        autoMeshSetAdvancedCommands[key] = cmd;
    }}
}
void ngInitCommands(T)() if (is(T == AutoMeshSetPresetKey))
{
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        AutoMeshSetPresetKey key = AutoMeshSetPresetKey(pid);
        alias C = mixin("AutoMeshSetPreset_" ~ _sanitizeId(__traits(identifier, PT)) ~ "Command");
        auto cmd = new C();
        ngRegisterCommandMeta(cmd);
        autoMeshSetPresetCommands[key] = cmd;
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
                auto cmd = new C();
                ngRegisterCommandMeta(cmd);
                autoMeshTypedCommands[KS] = cmd;
            }
        }
    }}
    size_t after = 0; foreach (_k, _v; autoMeshTypedCommands) ++after;
    cmdLog("[CMD] AutoMeshTypedCommand init: before=%s after=%s", before, after);
}
// Get currently active AutoMesh processor
@McpHidden
@GuiDialogOutput
class AutoMeshGetActiveCommand : ExCommand!() {
    this() { super(_("Get Active AutoMesh"), _("Show active AutoMesh processor")); }
    override bool runnable(Context ctx) { return true; }
    override CommandResult run(Context ctx) {
        import std.json : JSONValue, JSONType;
        auto p = ngActiveAutoMeshProcessor();
        if (p is null) return CommandResult(false, "No active AutoMesh processor");
        JSONValue o; o["id"] = p.procId(); o["name"] = p.displayName(); o["icon"] = p.icon(); o["reflectable"] = (cast(IAutoMeshReflect)p) !is null;
        incDialog(__("Active AutoMesh"), o.toString());
        return CommandResult(true);
    }
}

// Set active AutoMesh processor by id
@EffectConfigEdit
class AutoMeshSetActiveCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier")) {
    this(string id) { super(_("Set Active AutoMesh"), _("Activate AutoMesh processor"), id); }
    override bool runnable(Context ctx) { return true; }
    override CommandResult run(Context ctx) {
        foreach (pp; ngAutoMeshProcessors()) if (pp.procId() == processorId) { ngActiveAutoMeshProcessor(pp); return CommandResult(true); }
        incDialog(__("Error"), ("Processor not found: %s").format(processorId));
        return CommandResult(false, "Processor not found");
    }
}

// Apply active AutoMesh to current context/selection (reuses robust Apply command)
@EffectApply
class AutoMeshApplyActiveCommand : ExCommand!() {
    this() { super(_("Apply Active AutoMesh"), _("Apply active AutoMesh to targets")); }
    override bool runnable(Context ctx) { return true; }
    override CommandResult run(Context ctx) {
        auto p = ngActiveAutoMeshProcessor();
        if (p is null) return CommandResult(false, "No active AutoMesh processor");
        auto cmd = ensureApplyAutoMeshCommand(p.procId());
        if (cmd is null) return CommandResult(false, "Apply command missing");
        return cmd.run(ctx);
    }
}
