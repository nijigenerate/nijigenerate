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
import std.array : array;
import std.conv : to;
import std.algorithm : canFind;
import std.traits : isInstanceOf, TemplateArgsOf, isIntegral, isFloatingPoint, isSomeString, BaseClassesTuple, EnumMembers;
import std.stdio : writefln;

// nijigenerate command system
import nijigenerate.commands; // AllCommandMaps, Command, Context
import nijigenerate.core.shortcut.base : ngBuildExecutionContext;
import inmath : vec2u;
import nijigenerate.project : incActivePuppet;
import nijilive; // Node, Parameter, Puppet

// mcp-d
import mcp.server;
import mcp.transport.http : createHttpTransport;
import mcp.schema : SchemaBuilder;

// Simple queue item representing a command to run on the main thread
private struct EnqueuedCommand { void delegate() action; }

// Queue and synchronization
private __gshared EnqueuedCommand[] gQueue;
private __gshared Mutex gQueueMutex;
private __gshared bool gServerStarted = false;

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

            // Schema: optional context + reserved args object (future extension)
            auto ctxSchema = SchemaBuilder.object()
                .setDescription("Optional context. Omit or null to use active app state (like shortcuts).")
                .addProperty("parameters", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Parameter UUIDs (uint[])"))
                .addProperty("armedParameters", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Armed Parameter UUIDs (uint[])"))
                .addProperty("nodes", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("Node UUIDs (uint[])"))
                .addProperty("keyPoint", SchemaBuilder.array(SchemaBuilder.integer()).setDescription("vec2u [x,y] (uint,uint)"));

            // Build argument schema from ExCommand template parameters
            auto argSchema = SchemaBuilder.object().setDescription("Command arguments mapped from ExCommand template parameters.");
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
                            } else {
                                enum fname = "arg" ~ i.stringof;
                                alias TParam = Param;
                            }
                            static if (is(TParam == bool)) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.boolean());
                            } else static if (isIntegral!TParam || is(TParam == enum)) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.integer());
                            } else static if (isFloatingPoint!TParam) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.number());
                            } else static if (isSomeString!TParam) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.string_());
                            } else static if (is(TParam == vec2u)) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.integer()).setDescription("vec2u [x,y]"));
                            } else static if (is(TParam == vec3)) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.number()).setDescription("vec3 [x,y,z]"));
                            } else static if (is(TParam : Resource)) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.integer().setDescription("UUID of Resource"));
                            } else static if (is(TParam : Node)) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.integer().setDescription("UUID of Node"));
                            } else static if (is(TParam : Parameter)) {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.integer().setDescription("UUID of Parameter"));
                            } else {
                                argSchema = argSchema.addProperty(fname, SchemaBuilder.string_().setDescription("Unsupported type; pass as string"));
                            }
                            }}
                        }
                    }
                }}
            }}

            auto inputSchema = SchemaBuilder.object()
                .setDescription("Input for nijigenerate Command tool. 'context' is optional; omit or set to null to use active app state.")
                .addProperty("context", ctxSchema)
                .addProperty("args", argSchema);

            // Debug: log each tool registration
            writefln("[MCP] addTool: %s", toolName);
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

                        // Apply ExCommand arguments if provided
                        auto aobj = ("args" in payloadCopy && payloadCopy["args"].type == JSONType.object) ? payloadCopy["args"] : JSONValue.init;
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
                                            if (aobj.type == JSONType.object && fname in aobj) {
                                                auto val = aobj[fname];
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
            server.addTool(
                toolName,
                toolDesc.length ? toolDesc : ("Run command " ~ toolName),
                SchemaBuilder.object()  // no-arg schema for now; args handled inside main-thread action
                    .addProperty("context", SchemaBuilder.object()),
                (JSONValue payload) {
                    auto payloadCopy = payload;
                    _enqueueAction({
                        auto ctx = ngBuildExecutionContext();
                        if (cmdInst !is null && cmdInst.runnable(ctx)) cmdInst.run(ctx);
                    });
                    return JSONValue(["status": JSONValue("queued"), "id": JSONValue(toolName)]);
                }
            );
        }
    }

    auto t = new Thread({ server.start(); });
    t.isDaemon = true;
    t.start();
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
