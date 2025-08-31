module nijigenerate.api.mcp.server;
/**
 * MCP HTTP server integration for nijigenerate.
 *
 * Overview
 * - Starts a background MCP server (HTTP) that exposes one tool per registered Command
 * - Requests enqueue command execution; the main thread drains the queue and runs commands safely
 *
 * Context Semantics
 * - The input accepts an optional `context` object. If omitted or null, a default context is built
 *   from the current active app state (same as shortcuts).
 * - When provided, `context` supports the following keys:
 *   - `parameters`: array of uint UUIDs for Parameters
 *   - `armedParameters`: array of uint UUIDs for Parameters to arm
 *   - `nodes`: array of uint UUIDs for Nodes
 *   - `keyPoint`: [uint, uint] for vec2u
 * - Any missing key is treated as "not set" for that value (hasXXX=false in Context).
 */

import core.thread : Thread;
import core.sync.mutex : Mutex;
import std.json;
import std.array : array, join;
import std.conv : to;
import std.algorithm : canFind;
import std.traits : isInstanceOf, TemplateArgsOf, isIntegral, isFloatingPoint, isSomeString, BaseClassesTuple, EnumMembers;
import std.stdio : writefln;

// nijigenerate command system
import nijigenerate.commands; // AllCommandMaps, Command, Context
import nijigenerate.core.shortcut.base : ngBuildExecutionContext;
import inmath : vec2u;
import nijigenerate.project : incActivePuppet, incRegisterLoadFunc;
import nijilive; // Node, Parameter, Puppet

// mcp-d
import mcp.server;
import mcp.transport.http : createHttpTransport;
import mcp.schema : SchemaBuilder;
import mcp.prompts : PromptArgument, PromptResponse, PromptMessage; // proper prompt API
import mcp.resources : ResourceNotifier; // for resource change notifications
// Selector + Nodes
import nijigenerate.core.selector;
import nijigenerate.core.selector.resource : Resource, to;
import nijilive.core.nodes : Node, SerializeNodeFlags;
import nijilive.fmt.serialize : InochiSerializer, inCreateSerializer; 
import std.array : appender;
import std.json : parseJSON;
import core.sync.condition : Condition;

// Simple queue item representing a command to run on the main thread
private struct EnqueuedCommand { void delegate() action; }

// Queue and synchronization
private __gshared EnqueuedCommand[] gQueue;
private __gshared Mutex gQueueMutex;
private __gshared bool gServerStarted = false;
// Resource change notifiers (for MCP resources)
private __gshared ResourceNotifier gNotifyResourcesFind;
private __gshared ResourceNotifier gNotifyResourceByUuid;
private __gshared ResourceNotifier gNotifyResourceIndex;

// Resolve command instance by string id in the form "EnumType.Value"
private Command _resolveCommandByString(string id)
{
    Command result = null;
    static foreach (AA; AllCommandMaps) {
        foreach (k, v; AA) {
            string key = typeof(k).stringof ~ "." ~ to!string(k);
            if (key == id) {
                result = v;
            }
        }
    }
    return result;
}

// Enqueue a command run request
private void _enqueueAction(void delegate() action)
{
    synchronized (gQueueMutex) gQueue ~= EnqueuedCommand(action);
}

// Background MCP server thread
private void mcpServerThread(string host, ushort port)
{
    auto transport = createHttpTransport(host, port);
    auto server = new MCPServer(transport, "Nijigenerate MCP", "0.0.1");

    // Wait briefly until commands are initialized so enumeration yields results
    import core.time : msecs;
    import core.thread : Thread;
    foreach (_; 0 .. 40) { // up to ~2s
        size_t total = 0;
        static foreach (AA; AllCommandMaps) { total += AA.length; }
        if (_ == 0) writefln("[MCP] server thread sees %s commands initially", total);
        if (total > 0) break;
        Thread.sleep(50.msecs);
    }

    // Register one MCP tool per nijigenerate Command
    static foreach (AA; AllCommandMaps) {
        foreach (k, v; AA) {
            auto toolName = typeof(k).stringof ~ "." ~ to!string(k);
            auto toolDesc = v.description();
            // Extend description with common input guidance
            toolDesc ~= "\n\nInput: optional 'context'. Omit or set null to use active app state.\n"
                       ~ "Context keys: parameters(uint[]), armedParameters(uint[]), nodes(uint[]), keyPoint([x,y]).\n"
                       ~ "UUIDs are numeric. See prompts 'tools/guide' and 'selectors/guide'.";

            // Schema: optional context + command-specific parameters (top-level)
            auto ctxSchema = SchemaBuilder.object()
                .setDescription("Optional context. Omit or null to use active app state (like shortcuts).")
                .addProperty("parameters", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Parameter UUIDs (uint[])"))
                .addProperty("armedParameters", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Armed Parameter UUIDs (uint[])"))
                .addProperty("nodes", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Node UUIDs (uint[])"))
                .addProperty("keyPoint", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("vec2u [x,y] (uint,uint)"));

            // Build input schema and top-level parameters from ExCommand template parameters
            auto inputSchema = SchemaBuilder.object()
                .setDescription("Input for nijigenerate Command tool. Context is optional; other parameters map to command-specific arguments.")
                .addProperty("context", ctxSchema);
            string[] paramLog;
            // Helper to find ExCommand!T base and extract parameter list
            template BaseExArgsOf(alias C_) {
                alias _Bases = BaseClassesTuple!C_;
                private template _FindExArgs(Bases...) {
                    static if (Bases.length == 0) alias _FindExArgs = void;
                    else static if (isInstanceOf!(ExCommand, Bases[0])) alias _FindExArgs = TemplateArgsOf!(Bases[0]);
                    else alias _FindExArgs = _FindExArgs!(Bases[1 .. $]);
                }
                alias Picked = _FindExArgs!(_Bases);
                static if (is(Picked == void)) alias BaseExArgsOf = void; else alias BaseExArgsOf = Picked;
            }
            alias K = typeof(k);
            static if (is(K == enum)) static foreach (m; EnumMembers!K) {{
                if (k == m) {{
                    enum _mName  = __traits(identifier, m);
                    enum _typeName = _mName ~ "Command";
                    static if (__traits(compiles, mixin(_typeName))) {
                        alias C = mixin(_typeName);
                        static if (!is(BaseExArgsOf!C == void)) {
                            alias Declared = BaseExArgsOf!C;
                            static foreach (i, Param; Declared) {{
                                static if (isInstanceOf!(TW, Param)) {
                                    enum fname = TemplateArgsOf!Param[1];
                                    alias TParam = TemplateArgsOf!Param[0];
                                    enum fdesc = TemplateArgsOf!Param[2];
                                } else {
                                    enum fname = "arg" ~ i.stringof;
                                    alias TParam = Param;
                                    enum fdesc = "";
                                }
                                static if (is(TParam == bool)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.boolean().setDescription(fdesc));
                                    paramLog ~= fname ~ ":bool";
                                } else static if (isIntegral!TParam || is(TParam == enum)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription(fdesc));
                                    paramLog ~= fname ~ ":int";
                                } else static if (isFloatingPoint!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.number().setDescription(fdesc));
                                    paramLog ~= fname ~ ":number";
                                } else static if (isSomeString!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.string_().setDescription(fdesc));
                                    paramLog ~= fname ~ ":string";
                                } else static if (is(TParam == vec2u)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.integer()).setDescription((fdesc.length?fdesc~"; ":"")~"vec2u [x,y]"));
                                    paramLog ~= fname ~ ":vec2u";
                                } else static if (is(TParam == vec3)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.number()).setDescription((fdesc.length?fdesc~"; ":"")~"vec3 [x,y,z]"));
                                    paramLog ~= fname ~ ":vec3";
                            } else static if (is(TParam : Resource)) {
                                inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((fdesc.length?fdesc~"; ":"")~"UUID of Resource"));
                                paramLog ~= fname ~ ":ResourceUUID";
                            } else static if (is(TParam : Node)) {
                                inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((fdesc.length?fdesc~"; ":"")~"UUID of Node"));
                                paramLog ~= fname ~ ":NodeUUID";
                            } else static if (is(TParam : Parameter)) {
                                inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((fdesc.length?fdesc~"; ":"")~"UUID of Parameter"));
                                paramLog ~= fname ~ ":ParameterUUID";
                            } else static if (__traits(hasMember, TParam, "uuid")) {
                                // Generic UUID-carrying type
                                inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((fdesc.length?fdesc~"; ":"")~("UUID of " ~ TParam.stringof)));
                                paramLog ~= fname ~ ":UUID(" ~ TParam.stringof ~ ")";
                            } else {
                                inputSchema = inputSchema.addProperty(fname, SchemaBuilder.string_().setDescription((fdesc.length?fdesc~"; ":"")~"Unsupported type; pass as string"));
                                paramLog ~= fname ~ ":string(unknown)";
                            }
                            }}
                        }
                    }
                }}
            }}

            // Debug: log each tool registration
            writefln("[MCP] addTool: %s params=%s", toolName, paramLog);
            server.addTool(
                toolName,
                toolDesc.length ? toolDesc : ("Run command " ~ toolName),
                inputSchema,
                (JSONValue payload) {
                    // Capture inputs by value to avoid lifetime issues across threads
                    auto payloadCopy = payload; // JSONValue is a value type
                    auto baseCmd = v; // Command instance
                    _enqueueAction({
                        // Build context on main thread
                        Context ctx;
                        if ("context" in payloadCopy && payloadCopy["context"].type == JSONType.object) {
                            auto cobj = payloadCopy["context"];
                            ctx = new Context();
                            auto puppet = incActivePuppet();
                            if (puppet !is null) {
                                // parameters
                                if ("parameters" in cobj && cobj["parameters"].type == JSONType.array) {
                                    Parameter[] params;
                                    foreach (u; cobj["parameters"].array) {
                                        if (u.type == JSONType.integer) {
                                            auto p = puppet.find!(Parameter)(cast(uint)u.integer);
                                            if (p !is null) params ~= p;
                                        }
                                    }
                                    if (params.length) ctx.parameters = params;
                                }
                                // armedParameters
                                if ("armedParameters" in cobj && cobj["armedParameters"].type == JSONType.array) {
                                    Parameter[] aparams;
                                    foreach (u; cobj["armedParameters"].array) {
                                        if (u.type == JSONType.integer) {
                                            auto p = puppet.find!(Parameter)(cast(uint)u.integer);
                                            if (p !is null) aparams ~= p;
                                        }
                                    }
                                    if (aparams.length) ctx.armedParameters = aparams;
                                }
                                // nodes
                                if ("nodes" in cobj && cobj["nodes"].type == JSONType.array) {
                                    Node[] nodes;
                                    foreach (u; cobj["nodes"].array) {
                                        if (u.type == JSONType.integer) {
                                            auto n = puppet.find!(Node)(cast(uint)u.integer);
                                            if (n !is null) nodes ~= n;
                                        }
                                    }
                                    if (nodes.length) ctx.nodes = nodes;
                                }
                            }
                            // keyPoint
                            if ("keyPoint" in cobj && cobj["keyPoint"].type == JSONType.array && cobj["keyPoint"].array.length >= 2) {
                                auto a = cobj["keyPoint"].array;
                                if ((a[0].type == JSONType.integer || a[0].type == JSONType.float_)
                                 && (a[1].type == JSONType.integer || a[1].type == JSONType.float_)) {
                                    ctx.keyPoint = vec2u(cast(uint)(a[0].type == JSONType.integer ? a[0].integer : cast(long)a[0].floating),
                                                         cast(uint)(a[1].type == JSONType.integer ? a[1].integer : cast(long)a[1].floating));
                                }
                            }
                        } else {
                            ctx = ngBuildExecutionContext();
                        }

                        // Apply ExCommand arguments if provided (top-level)
                        alias K = typeof(k);
                        // Resolve concrete command type for this id value
                        static if (is(K == enum)) static foreach (m; EnumMembers!K) {{
                            if (k == m) {{
                                enum _mName  = __traits(identifier, m);
                                enum _typeName = _mName ~ "Command";
                                static if (!__traits(compiles, mixin(_typeName))) {
                                    if (baseCmd.runnable(ctx)) baseCmd.run(ctx);
                                } else {
                                alias C = mixin(_typeName);
                                auto inst = cast(C) baseCmd;
                                if (inst is null) {
                                    // fallback: just run without args
                                    if (baseCmd.runnable(ctx)) baseCmd.run(ctx);
                                } else {
                                    // Apply ExCommand arguments if provided
                                    static if (!is(BaseExArgsOf!C == void)) {
                                        alias Declared = BaseExArgsOf!C;
                                        static foreach (i, Param; Declared) {{
                                            static if (isInstanceOf!(TW, Param)) {
                                                enum fname = TemplateArgsOf!Param[1];
                                                alias TParam = TemplateArgsOf!Param[0];
                                            } else {
                                                enum fname = "arg" ~ i.stringof;
                                                alias TParam = Param;
                                            }
                                            if (fname in payloadCopy) {
                                                auto val = payloadCopy[fname];
                                                static if (is(TParam == bool)) {
                                                    if (val.type == JSONType.true_ || val.type == JSONType.false_) mixin("inst."~fname~" = (val.type==JSONType.true_);");
                                                } else static if (is(TParam == enum)) {
                                                    static if (__traits(compiles, cast(string) TParam.init)) {
                                                        if (val.type == JSONType.string) {
                                                            static foreach (mem; EnumMembers!TParam) {{
                                                                static if (__traits(compiles, cast(string)mem)) {
                                                                    enum string memStr = cast(string)mem;
                                                                    if (val.str == memStr) { mixin("inst."~fname~" = mem;"); }
                                                                }
                                                            }}
                                                        }
                                                    } else {
                                                        if (val.type == JSONType.integer) {
                                                            mixin("inst."~fname~" = cast(TParam) cast(int) val.integer;");
                                                        }
                                                    }
                                                } else static if (isIntegral!TParam) {
                                                    if (val.type == JSONType.integer) mixin("inst."~fname~" = cast(TParam) val.integer;");
                                                } else static if (isFloatingPoint!TParam) {
                                                    if (val.type == JSONType.float_) mixin("inst."~fname~" = cast(TParam) val.floating;");
                                                    else if (val.type == JSONType.integer) mixin("inst."~fname~" = cast(TParam) val.integer;");
                                                } else static if (isSomeString!TParam) {
                                                    if (val.type == JSONType.string) mixin("inst."~fname~" = val.str;");
                                                } else static if (is(TParam == vec2u)) {
                                                    if (val.type == JSONType.array && val.array.length >= 2) {
                                                        auto a = val.array;
                                                        uint x = cast(uint)(a[0].type==JSONType.integer ? a[0].integer : cast(long)a[0].floating);
                                                        uint y = cast(uint)(a[1].type==JSONType.integer ? a[1].integer : cast(long)a[1].floating);
                                                        mixin("inst."~fname~" = vec2u(x,y);");
                                                    }
                                                } else static if (is(TParam == vec3)) {
                                                    if (val.type == JSONType.array && val.array.length >= 3) {
                                                        auto a = val.array;
                                            float x = cast(float)(a[0].type==JSONType.float_ ? a[0].floating : cast(double)a[0].integer);
                                            float y = cast(float)(a[1].type==JSONType.float_ ? a[1].floating : cast(double)a[1].integer);
                                            float z = cast(float)(a[2].type==JSONType.float_ ? a[2].floating : cast(double)a[2].integer);
                                                        mixin("inst."~fname~" = vec3(x,y,z);");
                                                    }
                                                } else static if (is(TParam : Node)) {
                                                    if (val.type == JSONType.integer) {
                                                        auto n = incActivePuppet();
                                                        if (n !is null) {
                                                            auto nodeVal = n.find!(TParam)(cast(uint)val.integer);
                                                            if (nodeVal !is null) mixin("inst."~fname~" = nodeVal;");
                                                        }
                                                    }
                                                } else static if (is(TParam : Parameter)) {
                                                    if (val.type == JSONType.integer) {
                                                        auto n = incActivePuppet();
                                                        if (n !is null) {
                                                            auto pVal = n.find!(TParam)(cast(uint)val.integer);
                                                            if (pVal !is null) mixin("inst."~fname~" = pVal;");
                                                        }
                                                    }
                                                    } else static if (is(TParam : Resource)) {
                                                        if (val.type == JSONType.integer) {
                                                            auto n = incActivePuppet();
                                                            if (n !is null) {
                                                                auto nVal = n.find!(Node)(cast(uint)val.integer);
                                                                if (nVal !is null) mixin("inst."~fname~" = cast(TParam) nVal;");
                                                                else {
                                                                    auto pVal = n.find!(Parameter)(cast(uint)val.integer);
                                                                    if (pVal !is null) mixin("inst."~fname~" = cast(TParam) pVal;");
                                                                }
                                                            }
                                                        }
                                                    } else static if (__traits(hasMember, TParam, "uuid")) {
                                                        // Generic UUID-carrying type (best-effort via Puppet.find!TParam)
                                                        if (val.type == JSONType.integer) {
                                                            if (auto n = incActivePuppet()) {
                                                                static if (__traits(compiles, n.find!(TParam)(cast(uint)0))) {
                                                                    auto anyVal = n.find!(TParam)(cast(uint)val.integer);
                                                                    if (anyVal !is null) mixin("inst."~fname~" = anyVal;");
                                                                }
                                                            }
                                                        }
                                                    }
                                            }
                                        }}
                                    }
                                    if (baseCmd.runnable(ctx)) baseCmd.run(ctx);
                                }}
                            }}
                        }}
                    });

                    auto id = typeof(k).stringof ~ "." ~ to!string(k);
                    return JSONValue(["status": JSONValue("queued"), "id": JSONValue(id)]);
                }
            );
        }
    }

    server.start(); // blocks until closed
}

// Public: initialize and start MCP server in background
void ngMcpInit(string host = "127.0.0.1", ushort port = 8088)
{
    if (gServerStarted) return;
    gQueueMutex = new Mutex();
    gServerStarted = true;
    // Create server and register tools on the main thread to avoid TLS issues
    auto transport = createHttpTransport(host, port);
    auto server = new MCPServer(transport, "Nijigenerate MCP", "0.0.1");

    // Minimal, robust registration that does not depend on TLS of another thread
    bool[string] registered;
    static foreach (AA; AllCommandMaps) {
        foreach (k, v; AA) {
            auto baseName = typeof(k).stringof ~ "." ~ to!string(k);
            string toolName = baseName;
            // Disambiguate duplicates by appending command class name or counter
            if (toolName in registered) {
                import std.string : replace;
                auto cls = typeid(v).name.replace(".", "/");
                toolName = baseName ~ "@" ~ cls;
                size_t n = 1;
                while (toolName in registered) { toolName = baseName ~ "@" ~ cls ~ "#" ~ n.to!string; ++n; }
            }
            registered[toolName] = true;
            auto toolDesc = v.description();
            auto cmdInst = v; // capture concrete instance for execution

            // Build context schema
            auto ctxSchema = SchemaBuilder.object()
                .setDescription("Optional. Omit or null to use active app state. Keys: parameters, armedParameters, nodes, keyPoint")
                .addProperty("parameters", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Parameter UUIDs (uint[])"))
                .addProperty("armedParameters", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Armed Parameter UUIDs (uint[])"))
                .addProperty("nodes", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Node UUIDs (uint[])"))
                .addProperty("keyPoint", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("vec2u [x,y]"));

            // Helper to extract ExCommand template parameters
            template BaseExArgsOf(alias C_) {
                alias _Bases = BaseClassesTuple!C_;
                private template _FindExArgs(Bases...) {
                    static if (Bases.length == 0) alias _FindExArgs = void;
                    else static if (isInstanceOf!(ExCommand, Bases[0])) alias _FindExArgs = TemplateArgsOf!(Bases[0]);
                    else alias _FindExArgs = _FindExArgs!(Bases[1 .. $]);
                }
                alias Picked = _FindExArgs!(_Bases);
                static if (is(Picked == void)) alias BaseExArgsOf = void; else alias BaseExArgsOf = Picked;
            }

            // Build tool input schema with top-level parameters (separate from context)
            auto inputSchema = SchemaBuilder.object()
                .setDescription("Input for nijigenerate Command tool. Context is optional; other parameters map to command-specific arguments.")
                .addProperty("context", ctxSchema);

            string[] paramLog;
            alias K = typeof(k);
            static if (is(K == enum)) static foreach (m; EnumMembers!K) {{
                if (k == m) {{
                    enum _mName  = __traits(identifier, m);
                    enum _typeName = _mName ~ "Command";
                    static if (__traits(compiles, mixin(_typeName))) {
                        alias C = mixin(_typeName);
                        static if (!is(BaseExArgsOf!C == void)) {
                            alias Declared = BaseExArgsOf!C;
                            static foreach (i, Param; Declared) {{
                                static if (isInstanceOf!(TW, Param)) {
                                    enum fname = TemplateArgsOf!Param[1];
                                    alias TParam = TemplateArgsOf!Param[0];
                                    enum fdesc = TemplateArgsOf!Param[2];
                                } else {
                                    enum fname = "arg" ~ i.stringof;
                                    alias TParam = Param;
                                    enum fdesc = "";
                                }
                                static if (is(TParam == bool)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.boolean().setDescription(fdesc));
                                    paramLog ~= fname ~ ":bool";
                                } else static if (isIntegral!TParam || is(TParam == enum)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription(fdesc));
                                    paramLog ~= fname ~ ":int";
                                } else static if (isFloatingPoint!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.number().setDescription(fdesc));
                                    paramLog ~= fname ~ ":number";
                                } else static if (isSomeString!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.string_().setDescription(fdesc));
                                    paramLog ~= fname ~ ":string";
                                } else static if (is(TParam == vec2u)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.integer()).setDescription((fdesc.length?fdesc~"; ":"")~"vec2u [x,y]"));
                                    paramLog ~= fname ~ ":vec2u";
                                } else static if (is(TParam == vec3)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.number()).setDescription((fdesc.length?fdesc~"; ":"")~"vec3 [x,y,z]"));
                                    paramLog ~= fname ~ ":vec3";
                                } else static if (is(TParam : Resource)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((fdesc.length?fdesc~"; ":"")~"UUID of Resource"));
                                    paramLog ~= fname ~ ":ResourceUUID";
                                } else static if (is(TParam : Node)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((fdesc.length?fdesc~"; ":"")~"UUID of Node"));
                                    paramLog ~= fname ~ ":NodeUUID";
                                } else static if (is(TParam : Parameter)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((fdesc.length?fdesc~"; ":"")~"UUID of Parameter"));
                                    paramLog ~= fname ~ ":ParameterUUID";
                                } else {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.string_().setDescription((fdesc.length?fdesc~"; ":"")~"Unsupported type; pass as string"));
                                    paramLog ~= fname ~ ":string(unknown)";
                                }
                            }}
                        }
                    }
                }}
            }}

            // Log registration with parameters
            writefln("[MCP] addTool: %s params=[%s]", toolName, paramLog.join(", "));

            server.addTool(
                toolName,
                (toolDesc.length ? toolDesc : ("Run command " ~ toolName)) ~ "\n\nInput: optional 'context' (null to use active app state).",
                inputSchema,
                (JSONValue payload) {
                    // Debug: print incoming JSON for this tool call
                    writefln("[MCP] call %s: %s", toolName, payload.toString());
                    auto payloadCopy = payload;
                    _enqueueAction({
                        // 1) Start from default context
                        auto ctx = ngBuildExecutionContext();
                        // 2) Override only provided keys from context, if any
                        if ("context" in payloadCopy && payloadCopy["context"].type == JSONType.object) {
                            auto cobj = payloadCopy["context"];
                            auto puppet = incActivePuppet();
                            if (puppet !is null) {
                                if ("parameters" in cobj && cobj["parameters"].type == JSONType.array) {
                                    Parameter[] params;
                                    foreach (u; cobj["parameters"].array) {
                                        if (u.type == JSONType.integer) {
                                            auto p = puppet.find!(Parameter)(cast(uint)u.integer);
                                            if (p !is null) params ~= p;
                                        }
                                    }
                                    if (params.length) ctx.parameters = params;
                                }
                                if ("armedParameters" in cobj && cobj["armedParameters"].type == JSONType.array) {
                                    Parameter[] aparams;
                                    foreach (u; cobj["armedParameters"].array) {
                                        if (u.type == JSONType.integer) {
                                            auto p = puppet.find!(Parameter)(cast(uint)u.integer);
                                            if (p !is null) aparams ~= p;
                                        }
                                    }
                                    if (aparams.length) ctx.armedParameters = aparams;
                                }
                                if ("nodes" in cobj && cobj["nodes"].type == JSONType.array) {
                                    Node[] nodes;
                                    foreach (u; cobj["nodes"].array) {
                                        if (u.type == JSONType.integer) {
                                            auto n = puppet.find!(Node)(cast(uint)u.integer);
                                            if (n !is null) nodes ~= n;
                                        }
                                    }
                                    if (nodes.length) ctx.nodes = nodes;
                                }
                            }
                            if ("keyPoint" in cobj && cobj["keyPoint"].type == JSONType.array && cobj["keyPoint"].array.length >= 2) {
                                auto a = cobj["keyPoint"].array;
                                if ((a[0].type == JSONType.integer || a[0].type == JSONType.float_)
                                 && (a[1].type == JSONType.integer || a[1].type == JSONType.float_)) {
                                    ctx.keyPoint = vec2u(cast(uint)(a[0].type == JSONType.integer ? a[0].integer : cast(long)a[0].floating),
                                                         cast(uint)(a[1].type == JSONType.integer ? a[1].integer : cast(long)a[1].floating));
                                }
                            }
                        }
                        // 3) Apply command-specific parameters (top-level)
                        alias K = typeof(k);
                        static if (is(K == enum)) static foreach (m; EnumMembers!K) {{
                            if (k == m) {{
                                enum _mName  = __traits(identifier, m);
                                enum _typeName = _mName ~ "Command";
                                static if (__traits(compiles, mixin(_typeName))) {
                                    alias C = mixin(_typeName);
                                    if (auto inst = cast(C) cmdInst) {
                                        static if (!is(BaseExArgsOf!C == void)) {
                                            alias Declared = BaseExArgsOf!C;
                                            static foreach (i, Param; Declared) {{
                                                static if (isInstanceOf!(TW, Param)) {
                                                    enum fname = TemplateArgsOf!Param[1];
                                                    alias TParam = TemplateArgsOf!Param[0];
                                                } else {
                                                    enum fname = "arg" ~ i.stringof;
                                                    alias TParam = Param;
                                                }
                                                if (fname in payloadCopy) {
                                                    auto val = payloadCopy[fname];
                                                    static if (is(TParam == bool)) {
                                                        if (val.type == JSONType.true_ || val.type == JSONType.false_) mixin("inst."~fname~" = (val.type==JSONType.true_);");
                                                    } else static if (is(TParam == enum)) {
                                                        static if (__traits(compiles, cast(string) TParam.init)) {
                                                            if (val.type == JSONType.string) {
                                                                static foreach (mem; EnumMembers!TParam) {{
                                                                    static if (__traits(compiles, cast(string)mem)) {
                                                                        enum string memStr = cast(string)mem;
                                                                        if (val.str == memStr) { mixin("inst."~fname~" = mem;"); }
                                                                    }
                                                                }}
                                                            }
                                                        } else {
                                                            if (val.type == JSONType.integer) {
                                                                mixin("inst."~fname~" = cast(TParam) cast(int) val.integer;");
                                                            }
                                                        }
                                                    } else static if (isIntegral!TParam) {
                                                        if (val.type == JSONType.integer) mixin("inst."~fname~" = cast(TParam) val.integer;");
                                                    } else static if (isFloatingPoint!TParam) {
                                                        if (val.type == JSONType.float_) mixin("inst."~fname~" = cast(TParam) val.floating;");
                                                        else if (val.type == JSONType.integer) mixin("inst."~fname~" = cast(TParam) val.integer;");
                                                    } else static if (isSomeString!TParam) {
                                                        if (val.type == JSONType.string) mixin("inst."~fname~" = val.str;");
                                                    } else static if (is(TParam == vec2u)) {
                                                        if (val.type == JSONType.array && val.array.length >= 2) {
                                                            auto a = val.array;
                                                            uint x = cast(uint)(a[0].type==JSONType.integer ? a[0].integer : cast(long)a[0].floating);
                                                            uint y = cast(uint)(a[1].type==JSONType.integer ? a[1].integer : cast(long)a[1].floating);
                                                            mixin("inst."~fname~" = vec2u(x,y);");
                                                        }
                                                    } else static if (is(TParam == vec3)) {
                                                        if (val.type == JSONType.array && val.array.length >= 3) {
                                                            auto a = val.array;
                                                            float x = cast(float)(a[0].type==JSONType.float_ ? a[0].floating : cast(double)a[0].integer);
                                                            float y = cast(float)(a[1].type==JSONType.float_ ? a[1].floating : cast(double)a[1].integer);
                                                            float z = cast(float)(a[2].type==JSONType.float_ ? a[2].floating : cast(double)a[2].integer);
                                                            mixin("inst."~fname~" = vec3(x,y,z);");
                                                        }
                                                    } else static if (is(TParam : Node)) {
                                                        if (val.type == JSONType.integer) {
                                                            if (auto puppet = incActivePuppet()) {
                                                                auto nodeVal = puppet.find!(TParam)(cast(uint)val.integer);
                                                                if (nodeVal !is null) mixin("inst."~fname~" = nodeVal;");
                                                            }
                                                        }
                                                    } else static if (is(TParam : Parameter)) {
                                                        if (val.type == JSONType.integer) {
                                                            if (auto puppet = incActivePuppet()) {
                                                                auto pVal = puppet.find!(TParam)(cast(uint)val.integer);
                                                                if (pVal !is null) mixin("inst."~fname~" = pVal;");
                                                            }
                                                        }
                                                    } else static if (is(TParam : Resource)) {
                                                        if (val.type == JSONType.integer) {
                                                            if (auto puppet = incActivePuppet()) {
                                                                auto nVal = puppet.find!(Node)(cast(uint)val.integer);
                                                                if (nVal !is null) mixin("inst."~fname~" = cast(TParam) nVal;");
                                                                else {
                                                                    auto pVal = puppet.find!(Parameter)(cast(uint)val.integer);
                                                                    if (pVal !is null) mixin("inst."~fname~" = cast(TParam) pVal;");
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }}
                                        }
                                    }
                                }
                            }}
                        }}

                        // 4) Run the captured command instance with the prepared context
                        if (cmdInst !is null && cmdInst.runnable(ctx)) cmdInst.run(ctx);
                    });
                    return JSONValue(["status": JSONValue("queued"), "id": JSONValue(toolName)]);
                }
            );
        }
    }

    // Register resources/find as a Tool (returns JSON)
    writefln("[MCP] addTool: resources/find (special)");
    server.addTool(
        "resources/find",
        "Find resources by selector and return Basics + Children only.",
        SchemaBuilder.object()
            .addProperty("selector", SchemaBuilder.string_().setDescription("Selector string (nijigenerate.core.selector)")),
        (JSONValue payload) {
            writefln("[MCP] call resources/find: %s", payload.toString());
            auto payloadCopy = payload;
            auto lock = new Mutex();
            auto cond = new Condition(lock);
            bool done = false;
            JSONValue result;

            _enqueueAction({
                SerializeNodeFlags flags = SerializeNodeFlags.Basics; // fixed
                string selectorParam = ("selector" in payloadCopy && payloadCopy["selector"].type == JSONType.string) ? payloadCopy["selector"].str : "";
                writefln("[MCP resources/find] selector='%s' (Basics + Children)", selectorParam);
                Selector sel = new Selector();
                if (selectorParam.length) sel.build(selectorParam);
                auto results = sel.run();

                import nijigenerate.core.selector.treestore : TreeStore;
                auto ts = new TreeStore();
                ts.setResources(results);

                JSONValue makeTree(Resource res) {
                    JSONValue[string] map;
                    map["typeId"] = JSONValue(res.typeId);
                    map["uuid"] = JSONValue(cast(long)res.uuid);
                    map["name"] = JSONValue(res.name);

                    auto node = nijigenerate.core.selector.resource.to!Node(res);
                    if (node !is null) {
                        auto app = appender!(char[]);
                        auto ser = inCreateSerializer(app);
                        auto st = ser.structBegin();
                        node.serializePartial(ser, flags, true);
                        ser.structEnd(st);
                        ser.flush();
                        map["data"] = parseJSON(cast(string)app.data);
                    }

                    JSONValue[] childArr;
                    if (res in ts.children) foreach (child; ts.children[res]) childArr ~= makeTree(child);
                    map["children"] = JSONValue(childArr);
                    return JSONValue(map);
                }

                JSONValue[] rootsOut;
                foreach (r; ts.roots) rootsOut ~= makeTree(r);

                synchronized (lock) {
                    result = JSONValue(["items": JSONValue(rootsOut)]);
                    done = true;
                    cond.notify();
                }
            });
            synchronized (lock) { while (!done) cond.wait(); }
            return result;
        }
    );

    // Register resources/get as a Tool (returns JSON)
    writefln("[MCP] addTool: resources/get (special)");
    server.addTool(
        "resources/get",
        "Get a single resource by UUID. Returns Basics + State + Geometry + Links for Nodes; basics for Parameters.",
        SchemaBuilder.object()
            .addProperty("uuid", SchemaBuilder.integer().setDescription("UUID of resource (numeric)")),
        (JSONValue payload) {
            writefln("[MCP] call resources/get: %s", payload.toString());
            auto payloadCopy = payload;
            auto lock = new Mutex();
            auto cond = new Condition(lock);
            bool done = false;
            JSONValue result;

            _enqueueAction({
                uint uuid = 0;
                if ("uuid" in payloadCopy && payloadCopy["uuid"].type == JSONType.integer) uuid = cast(uint) payloadCopy["uuid"].integer;
                SerializeNodeFlags flags = SerializeNodeFlags.Basics | SerializeNodeFlags.State | SerializeNodeFlags.Geometry | SerializeNodeFlags.Links;

                JSONValue[string] map;
                auto puppet = incActivePuppet();
                if (puppet !is null && uuid != 0) {
                    if (auto node = puppet.find!(Node)(uuid)) {
                        map["typeId"] = JSONValue("Node");
                        map["uuid"] = JSONValue(cast(long)uuid);
                        map["name"] = JSONValue(node.name);
                        auto app = appender!(char[]);
                        auto ser = inCreateSerializer(app);
                        auto st = ser.structBegin();
                        node.serializePartial(ser, flags, true);
                        ser.structEnd(st);
                        ser.flush();
                        map["data"] = parseJSON(cast(string)app.data);
                    } else if (auto param = puppet.find!(Parameter)(uuid)) {
                        map["typeId"] = JSONValue("Parameter");
                        map["uuid"] = JSONValue(cast(long)uuid);
                        map["name"] = JSONValue(param.name);
                    }
                }
                JSONValue obj = map.length ? JSONValue(map) : JSONValue(null);
                synchronized (lock) {
                    result = JSONValue(["item": obj]);
                    done = true;
                    cond.notify();
                }
            });
            synchronized (lock) { while (!done) cond.wait(); }
            return result;
        }
    );

    // Also expose as Resources (for clients expecting MCP resources)
    // Dynamic resource: resource://nijigenerate/resources/find?selector=...
    writefln("[MCP] addDynamicResource: %s", "resource://nijigenerate/resources/find?");
    gNotifyResourcesFind = server.addDynamicResource(
        "resource://nijigenerate/resources/find?",
        "Find Resources",
        "Find resources by selector. Returns JSON tree with Basics + Children.",
        (string path) {
            // path is the query string after '?', e.g. "selector=..." (URL-encoded)
            writefln("[MCP] read resource resources/find?%s", path);
            string selectorParam;
            // Minimal query parser (only 'selector' supported)
            import std.string : split;
            foreach (pair; path.split('&')) {
                auto kv = pair.split('=');
                if (kv.length >= 1 && kv[0] == "selector") {
                    import std.uri : decodeComponent;
                    selectorParam = (kv.length >= 2) ? decodeComponent(kv[1]) : "";
                }
            }

            // Block until main-thread serialization completes
            auto lock = new Mutex();
            auto cond = new Condition(lock);
            bool done = false;
            JSONValue result;

            _enqueueAction({
                SerializeNodeFlags flags = SerializeNodeFlags.Basics; // fixed
                Selector sel = new Selector();
                if (selectorParam.length) sel.build(selectorParam);
                auto results = sel.run();

                import nijigenerate.core.selector.treestore : TreeStore;
                auto ts = new TreeStore();
                ts.setResources(results);

                JSONValue makeTree(Resource res) {
                    JSONValue[string] map;
                    map["typeId"] = JSONValue(res.typeId);
                    map["uuid"] = JSONValue(cast(long)res.uuid);
                    map["name"] = JSONValue(res.name);

                    auto node = nijigenerate.core.selector.resource.to!Node(res);
                    if (node !is null) {
                        auto app = appender!(char[]);
                        auto ser = inCreateSerializer(app);
                        auto st = ser.structBegin();
                        node.serializePartial(ser, flags, true);
                        ser.structEnd(st);
                        ser.flush();
                        map["data"] = parseJSON(cast(string)app.data);
                    }

                    JSONValue[] childArr;
                    if (res in ts.children) foreach (child; ts.children[res]) childArr ~= makeTree(child);
                    map["children"] = JSONValue(childArr);
                    return JSONValue(map);
                }

                JSONValue[] rootsOut;
                foreach (r; ts.roots) rootsOut ~= makeTree(r);

                synchronized (lock) {
                    result = JSONValue(["items": JSONValue(rootsOut)]);
                    done = true;
                    cond.notify();
                }
            });
            synchronized (lock) { while (!done) cond.wait(); }

            import mcp.resources : ResourceContents;
            return ResourceContents.makeText("application/json", result.toString());
        }
    );

    // Template resource: resource://nijigenerate/resources/{uuid}
    writefln("[MCP] addTemplate: %s", "resource://nijigenerate/resources/{uuid}");
    gNotifyResourceByUuid = server.addTemplate(
        "resource://nijigenerate/resources/{uuid}",
        "Get Resource",
        "Get a single resource by UUID. Returns Basics + State + Geometry + Links for Nodes; basics for Parameters.",
        "application/json",
        (string[string] params) {
            import std.conv : to;
            uint uuid = 0;
            if ("uuid" in params) {
                string v = params["uuid"];
                try {
                    if (v.length > 2 && (v[0..2] == "0x" || v[0..2] == "0X")) uuid = cast(uint) to!ulong(v);
                    else uuid = to!uint(v);
                } catch (Exception) {}
            }

            // Block until main-thread serialization completes
            auto lock = new Mutex();
            auto cond = new Condition(lock);
            bool done = false;
            JSONValue result;

            _enqueueAction({
                SerializeNodeFlags flags = SerializeNodeFlags.Basics | SerializeNodeFlags.State | SerializeNodeFlags.Geometry | SerializeNodeFlags.Links;
                JSONValue[string] map;
                auto puppet = incActivePuppet();
                if (puppet !is null && uuid != 0) {
                    if (auto node = puppet.find!(Node)(uuid)) {
                        map["typeId"] = JSONValue("Node");
                        map["uuid"] = JSONValue(cast(long)uuid);
                        map["name"] = JSONValue(node.name);
                        auto app = appender!(char[]);
                        auto ser = inCreateSerializer(app);
                        auto st = ser.structBegin();
                        node.serializePartial(ser, flags, true);
                        ser.structEnd(st);
                        ser.flush();
                        map["data"] = parseJSON(cast(string)app.data);
                    } else if (auto param = puppet.find!(Parameter)(uuid)) {
                        map["typeId"] = JSONValue("Parameter");
                        map["uuid"] = JSONValue(cast(long)uuid);
                        map["name"] = JSONValue(param.name);
                    }
                }
                JSONValue obj = map.length ? JSONValue(map) : JSONValue(null);
                synchronized (lock) {
                    result = JSONValue(["item": obj]);
                    done = true;
                    cond.notify();
                }
            });
            synchronized (lock) { while (!done) cond.wait(); }

            import mcp.resources : ResourceContents;
            return ResourceContents.makeText("application/json", result.toString());
        }
    );

    // Static resource: index of all UUID-based resource URIs for current puppet
    writefln("[MCP] addResource: %s", "resource://nijigenerate/resources/index");
    gNotifyResourceIndex = server.addResource(
        "resource://nijigenerate/resources/index",
        "Resource Index",
        "List of UUID-based resource URIs for current puppet.",
        () {
            Selector sel = new Selector();
            sel.build("Node, Parameter");
            auto results = sel.run();
            JSONValue[] items;
            foreach (res; results) {
                auto uri = "resource://nijigenerate/resources/" ~ to!string(res.uuid);
                JSONValue[string] m;
                m["uri"] = JSONValue(uri);
                m["uuid"] = JSONValue(cast(long)res.uuid);
                m["typeId"] = JSONValue(res.typeId);
                m["name"] = JSONValue(res.name);
                items ~= JSONValue(m);
            }
            auto payload = JSONValue(["items": JSONValue(items)]);
            import mcp.resources : ResourceContents;
            return ResourceContents.makeText("application/json", payload.toString());
        }
    );

    auto t = new Thread({ server.start(); });
    t.isDaemon = true;
    t.start();

    // Notify MCP clients that resources have changed when a project is loaded
    incRegisterLoadFunc((puppet) {
        if (gNotifyResourcesFind) gNotifyResourcesFind();
        if (gNotifyResourceByUuid) gNotifyResourceByUuid();
        if (gNotifyResourceIndex) gNotifyResourceIndex();
    });

    // Register helpful prompts (best-effort; only if SDK supports prompts)
    enum string RES_FIND_PROMPT =
        "Tool: resources/find\n" ~
        "Purpose: Hierarchical exploration ONLY. Use selector to locate resources and their UUIDs. For reading content, use resources/{uuid}. Returns Basics + Children tree.\n\n" ~
        "Input fields:\n" ~
        "- selector (string): nijigenerate.core.selector query\n\n" ~
        "Examples:\n" ~
        "1) List all nodes (basic info)\n" ~
        "   selector=Node\n" ~
        "2) By name (Part named \"Eye\")\n" ~
        "   selector=Part.\"Eye\"\n" ~
        "3) Direct children Parts of any Node\n" ~
        "   selector=Node > Part\n" ~
        "4) Active bindings of selected node\n" ~
        "   selector=Binding:active\n\n" ~
        "Official read endpoint:\n" ~
        "- resource://nijigenerate/resources/{uuid} (see resources/get)\n";

    enum string RESOURCE_PROMPT =
        "Tool: resources/get\n" ~
        "Purpose: OFFICIAL read endpoint. Fetch one resource by UUID. Node returns Basics + State + Geometry + Links; Parameter returns Basics.\n\n" ~
        "Input fields:\n" ~
        "- uuid (integer): Numeric UUID of a Node or Parameter.\n\n" ~
        "Examples:\n" ~
        "- Get a node by UUID\n" ~
        "  uuid=123456\n";

    enum string SELECTOR_GUIDE =
        "Selector Syntax (nijigenerate.core.selector)\n\n" ~
        "Overview:\n" ~
        "- Pattern: [TypeId|*] [ .name | #UUID ] [ [attr=value] ] [ :pseudo(args) ] [ combinators ]\n" ~
        "- Combinators: A > B (child), A B (descendant), A, B (union)\n\n" ~
        "Types:\n" ~
        "- Common: Root, Node, Part, Parameter, Binding, Group, * (any)\n\n" ~
        "Basic selectors:\n" ~
        "- .name              → match by name (string). Use quotes for spaces or non-ASCII.\n" ~
        "- #UUID              → match by numeric UUID (decimal).\n" ~
        "- [name=\"...\"]     → attribute equals (string).\n" ~
        "- [uuid=123]         → attribute equals (numeric).\n\n" ~
        "Quoting & escaping:\n" ~
        "- Use double quotes for names with spaces: Part.\"Left Eye\"\n" ~
        "- Escape quotes with \\\": Part.\\\"\"Quoted\"\\\"\n\n" ~
        "Pseudoclasses (examples):\n" ~
        "- Binding:active     → bindings on the currently armed parameter.\n\n" ~
        "Combinator examples:\n" ~
        "- Node > Part        → direct Part children of any Node.\n" ~
        "- Node Part          → any descendant Part under any Node.\n" ~
        "- Node, Part         → all Nodes and Parts.\n\n" ~
        "Advanced examples:\n" ~
        "- Part.#123          → Part with UUID 123.\n" ~
        "- Part[name=\"Eye\"] → Part whose name is \"Eye\".\n" ~
        "- Binding:active     → only active bindings for the armed parameter.\n\n" ~
        "Tips:\n" ~
        "- Prefer #UUID for precise targeting.\n" ~
        "- Build lists with resources/find, then fetch details by UUID.\n";

    enum string RESOURCES_GUIDE =
        "Resources vs Tools (Official Policy)\n\n" ~
        "Policy:\n" ~
        "- Exploration: Use resources/find (hierarchical traversal only).\n" ~
        "- Reading:    Use resources/{uuid} (official endpoint) to fetch content.\n" ~
        "- Index:      'resource://nijigenerate/resources/index' returns a UUID-based URI list for the current puppet.\n" ~
        "- Tools:      Use Tool commands to mutate or perform actions.\n\n" ~
        "URIs:\n" ~
        "- resource://nijigenerate/resources/find?selector=... (exploration)\n" ~
        "- resource://nijigenerate/resources/{uuid} (read)\n" ~
        "- resource://nijigenerate/resources/index (list UUID-based URIs)\n\n" ~
        "Typical flow:\n" ~
        "1) Explore with resources/find to identify targets and get UUIDs.\n" ~
        "2) Read details via resources/{uuid}.\n" ~
        "3) Optionally call Tools to modify.\n\n" ~
        "Notes:\n" ~
        "- 'resources/list' lists definitions (like find?* and templates), not instances. Use 'resources/index' or find to enumerate UUIDs.\n" ~
        "- See also: 'selectors/guide' and 'tools/guide'.";

    enum string TOOLS_GUIDE =
        "Tools Input & Context\n\n" ~
        "Context (recommended null):\n" ~
        "- Omit or set to null to use the active app state (same as shortcuts).\n" ~
        "- Provide only when needed to override selection.\n\n" ~
        "Context keys:\n" ~
        "- parameters: uint[] of Parameter UUIDs.\n" ~
        "- armedParameters: uint[] of Parameter UUIDs to arm.\n" ~
        "- nodes: uint[] of Node UUIDs.\n" ~
        "- keyPoint: [x,y] (uint,uint).\n\n" ~
        "Other parameters:\n" ~
        "- Unless a tool defines additional properties, only 'context' is accepted in this build.\n" ~
        "- UUIDs are numeric (decimal). Obtain them via resources/find or resources/{uuid}.\n\n" ~
        "Examples (JSON args):\n" ~
        "- Run with default context: {\"context\": null}\n" ~
        "- Run for specific nodes: {\"context\": {\"nodes\":[123,456]}}\n";

    // Register prompts using the proper API (no static-if fallbacks)
    writefln("[MCP] addPrompt: resources/find");
    server.addPrompt(
        "resources/find",
        "How to use the resources/find tool",
        cast(PromptArgument[])[],
        (string name, string[string] args){
            return PromptResponse(
                "Guide for resources/find",
                [PromptMessage.text("assistant", RES_FIND_PROMPT)]
            );
        }
    );

    writefln("[MCP] addPrompt: resources/get");
    server.addPrompt(
        "resources/get",
        "How to use resources/get tool",
        cast(PromptArgument[])[],
        (string name, string[string] args){
            return PromptResponse(
                "Guide for resources/get",
                [PromptMessage.text("assistant", RESOURCE_PROMPT)]
            );
        }
    );

    writefln("[MCP] addPrompt: selectors/guide");
    server.addPrompt(
        "selectors/guide",
        "Selector syntax and examples",
        cast(PromptArgument[])[],
        (string name, string[string] args){
            return PromptResponse(
                "Selector syntax",
                [PromptMessage.text("assistant", SELECTOR_GUIDE)]
            );
        }
    );

    writefln("[MCP] addPrompt: resources/guide");
    server.addPrompt(
        "resources/guide",
        "Resources usage guide",
        cast(PromptArgument[])[],
        (string name, string[string] args){
            return PromptResponse(
                "Resources policy",
                [PromptMessage.text("assistant", RESOURCES_GUIDE)]
            );
        }
    );

    writefln("[MCP] addPrompt: tools/guide");
    server.addPrompt(
        "tools/guide",
        "Tools input and context guide",
        cast(PromptArgument[])[],
        (string name, string[string] args){
            return PromptResponse(
                "Tools usage",
                [PromptMessage.text("assistant", TOOLS_GUIDE)]
            );
        }
    );
}

// Public: process pending queue items on the main thread
void ngMcpProcessQueue()
{
    EnqueuedCommand[] items;
    synchronized (gQueueMutex) {
        if (gQueue.length == 0) return;
        items = gQueue;
        gQueue.length = 0;
    }

    foreach (item; items) {
        if (item.action !is null) {
            try item.action(); catch (Exception) {}
        }
    }
}
