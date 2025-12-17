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
import std.json;
import std.array : array, join;
import std.conv : to;
import std.algorithm : canFind;
import std.traits : isInstanceOf, TemplateArgsOf, isIntegral, isFloatingPoint, isSomeString, BaseClassesTuple, EnumMembers, ReturnType;
import std.stdio : writefln;

// nijigenerate command system
import nijigenerate.commands; // AllCommandMaps, Command, Context
import nijigenerate.commands.base : BaseExArgsOf, enrichArgDesc, ngCommandIdFromKey;
import nijigenerate.commands.automesh.config : AutoMeshTypedCommand; // for CT logs
import nijigenerate.project : incActivePuppet, incRegisterLoadFunc;
import nijilive; // Node, Parameter, Puppet
import nijilive.core.param.binding : ParameterBinding;
import nijigenerate.ext.param : ExParameterGroup;

// mcp-d
import mcp.server;
import mcp.schema : SchemaBuilder;
import mcp.prompts : PromptArgument, PromptResponse, PromptMessage; // proper prompt API
import mcp.resources : ResourceNotifier; // for resource change notifications
import nijigenerate.api.mcp.http_transport;
// Selector + Nodes
import nijigenerate.core.selector;
import nijigenerate.core.selector.resource : Resource, to;
import nijilive.core.nodes : Node, SerializeNodeFlags;
import nijilive.fmt.serialize : InochiSerializer, inCreateSerializer; 
import std.array : appender;
import std.json : parseJSON;
public import nijigenerate.api.mcp.task;
import nijigenerate.core.settings;
import std.meta : AliasSeq;

// helpers
import nijigenerate.api.mcp.helpers : commandResultToJson, buildContextFromPayload, applyPayloadToInstance;

// Debug logging (compiled out in non-debug builds)
private void mcpLog(T...)(T args) {
    version(MCP_LOG) writefln(args);
}

private bool _mcpValidToolName(string s) {
    import std.uni : isAlphaNum;
    if (s.length == 0) return false;
    foreach (ch; s) {
        if (!(isAlphaNum(ch) || ch == '_' || ch == '-')) return false;
    }
    return true;
}

// === Compile-time diagnostics ===
// KeyType extractor for AA like V[K]
private template _McpKeyTypeOfAA(alias AA) {
    static if (is(typeof(AA) : V[K], V, K)) alias _McpKeyTypeOfAA = K;
    else alias _McpKeyTypeOfAA = void;
}

// List AllCommandMaps entries (type + key type) at compile time
static foreach (AA; AllCommandMaps) {
//    pragma(msg, "[CT][MCP] AllCommandMaps entry: " ~ typeof(AA).stringof ~ " Key=" ~ _McpKeyTypeOfAA!(AA).stringof);
}

// List AutoMeshTypedCommand enum members (if any) at compile time
private string _mcpListTypedMembers()() {
    string s;
    static foreach (n; __traits(allMembers, AutoMeshTypedCommand)) {
        static if (n != "init" && n != "min" && n != "max" && n != "stringof") {
            s ~= n ~ ",";
        }
    }
    return s;
}
//pragma(msg, "[CT][MCP] AutoMeshTypedCommand members: " ~ _mcpListTypedMembers());

private __gshared bool gServerStarted = false;
private __gshared Thread gServerThread;
private __gshared string gServerHost = "127.0.0.1";
private __gshared ushort gServerPort = 8088;
private __gshared bool gServerEnabled = false;
private __gshared ExtMCPServer gServerInstance = null;
private __gshared HttpTransport gTransport = null;
// Resource change notifiers (for MCP resources)
private __gshared ResourceNotifier gNotifyResourcesFind;
private __gshared ResourceNotifier gNotifyResourceByUuid;
private __gshared ResourceNotifier gNotifyResourceIndex;

// Resolve command instance by string id in the form "EnumType.Value"
private Command _resolveCommandByString(string id) {
    Command result = null;
    static foreach (AA; AllCommandMaps) {
        foreach (k, v; AA) {
            auto key = ngCommandIdFromKey(k);
            auto legacyKey = typeof(k).stringof ~ "." ~ to!string(k);
            if (key == id || legacyKey == id) {
                result = v;
            }
        }
    }
    return result;
}

// Internal: start server (assumes not started)
private void _ngMcpStart(string host, ushort port) {
    if (gServerStarted) return;
    ngMcpInitTask();
    gServerStarted = true;
    gServerEnabled = true;
    gServerHost = host;
    gServerPort = port;
    mcpLog("[MCP] starting server host=%s port=%s", host, port);
    // Create server and register tools on the main thread to avoid TLS issues
    auto transport = createHttpTransport(host, port);
    auto server = new ExtMCPServer(transport, "Nijigenerate MCP", "0.0.1");
    gServerInstance = server;
    gTransport = transport;
    ngMcpAuthEnabled(incSettingsGet!bool("MCP.authEnabled", false));

    // Ensure commands are initialized (safety in case caller forgot)
    import nijigenerate.commands : ngInitAllCommands;
    ngInitAllCommands();

    // Minimal, robust registration that does not depend on TLS of another thread
    bool[string] registered;
    static foreach (AA; AllCommandMaps) {{
        size_t _aaAdded = 0;
        foreach (k, v; AA) {
            // Build a spec-compliant tool name: only [a-zA-Z0-9_-]
            string makeSafe(string s) {
                import std.uni : isAlphaNum;
                auto app = appender!string();
                foreach (ch; s) app ~= (isAlphaNum(ch) || ch == '_' || ch == '-') ? ch : '_';
                return app.data;
            }

            auto toolDesc = v.description();
            auto cmdInst = v; // capture concrete instance for execution
            auto newBaseName = ngCommandIdFromKey(k);
            auto legacyBaseName = typeof(k).stringof ~ "." ~ to!string(k);
            string[] baseNames = (legacyBaseName != newBaseName) ? [newBaseName, legacyBaseName] : [newBaseName];

            // Build tool input schema (command args only). Context is allowed as an additional property (any type),
            // so that clients may pass null or omit it entirely. The handler will ignore non-object contexts.
            auto inputSchema = SchemaBuilder.object()
                .setDescription("Input for nijigenerate Command tool. Context is optional (omit or null); other parameters map to command-specific arguments.")
                .allowAdditional(true);

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
                                enum desc = enrichArgDesc!TParam(fdesc);
                                static if (is(TParam == bool)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.boolean().setDescription(desc));
                                    paramLog ~= fname ~ ":bool";
                                } else static if (isIntegral!TParam || is(TParam == enum)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription(desc));
                                    paramLog ~= fname ~ ":int";
                                } else static if (isFloatingPoint!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.number().setDescription(desc));
                                    paramLog ~= fname ~ ":number";
                                } else static if (isSomeString!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.string_().setDescription(desc));
                                    paramLog ~= fname ~ ":string";
                                } else static if (is(TParam == vec2u)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.integer()).setDescription((desc.length?desc~"; ":"")~"vec2u [x,y]"));
                                    paramLog ~= fname ~ ":vec2u";
                                } else static if (is(TParam == vec3)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.number()).setDescription((desc.length?desc~"; ":"")~"vec3 [x,y,z]"));
                                    paramLog ~= fname ~ ":vec3";
                                } else static if (is(TParam == float[])) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.number()).setDescription((desc.length?desc~"; ":"")~"float[]"));
                                    paramLog ~= fname ~ ":float[]";
                                } else static if (is(TParam == float[2])) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.number()).setDescription((desc.length?desc~"; ":"")~"float[2] [x,y]"));
                                    paramLog ~= fname ~ ":float[2]";
                                } else static if (is(TParam == float[3])) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.number()).setDescription((desc.length?desc~"; ":"")~"float[3] [x,y,z]"));
                                    paramLog ~= fname ~ ":float[3]";
                                } else static if (is(TParam == ushort[])) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.integer()).setDescription((desc.length?desc~"; ":"")~"ushort[]"));
                                    paramLog ~= fname ~ ":float[]";
                                } else static if (is(TParam == uint[2])) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.integer()).setDescription((desc.length?desc~"; ":"")~"uint[2] [x,y]"));
                                    paramLog ~= fname ~ ":uint[2]";
                                } else static if (is(TParam : Resource)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((desc.length?desc~"; ":"")~"UUID of Resource"));
                                    paramLog ~= fname ~ ":ResourceUUID";
                                } else static if (is(TParam : Node)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((desc.length?desc~"; ":"")~"UUID of Node"));
                                    paramLog ~= fname ~ ":NodeUUID";
                                } else static if (is(TParam : Parameter)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription((desc.length?desc~"; ":"")~"UUID of Parameter"));
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

            foreach (baseName; baseNames) {
                string toolName = makeSafe(baseName);
                auto toolNameCaptured = toolName;

                // Disambiguate duplicates by appending command class name or counter (also sanitized)
                if (toolNameCaptured in registered) {
                    auto cls = makeSafe(typeid(v).name);
                    toolNameCaptured = toolNameCaptured ~ "_" ~ cls;
                    size_t n = 1;
                    while (toolNameCaptured in registered) { toolNameCaptured = toolNameCaptured ~ "_" ~ n.to!string; ++n; }
                }
                if (!_mcpValidToolName(toolNameCaptured)) {
                    mcpLog("[MCP][WARN] skip tool with invalid sanitized name: '%s' (base='%s')", toolNameCaptured, baseName);
                    continue;
                }
                registered[toolNameCaptured] = true;
                ++_aaAdded;
                auto toolNameLocal = toolNameCaptured;

                // Log registration with parameters
                mcpLog("[MCP] addTool: %s params=[%s]", toolNameLocal, paramLog.join(", "));

                server.addTool(
                    toolNameLocal,
                    (toolDesc.length ? toolDesc : ("Run command " ~ toolNameLocal)) ~ "\n\nInput: optional 'context' (null to use active app state).",
                    inputSchema,
                    (JSONValue payload) {
                        // Debug: print incoming JSON for this tool call
                        mcpLog("[MCP] call %s: %s", toolNameLocal, payload.toString());
                        auto payloadCopy = payload;
                        return ngRunInMainThread({
                            // 1) Build context from payload
                            auto ctx = buildContextFromPayload(payloadCopy);
                            // 2) Apply command-specific parameters (top-level)
                            alias K = typeof(k);
                            static if (is(K == enum)) static foreach (m; EnumMembers!K) {{
                                if (k == m) {{
                                    enum _mName  = __traits(identifier, m);
                                    enum _typeName = _mName ~ "Command";
                                    static if (__traits(compiles, mixin(_typeName))) {
                                        alias C = mixin(_typeName);
                                        if (auto inst = cast(C) cmdInst) {
                                            applyPayloadToInstance(inst, payloadCopy);
                                        }
                                    }
                                }}
                            }}

                            // 3) Run the captured command instance with the prepared context
                            if (cmdInst !is null && cmdInst.runnable(ctx)) {
                                alias RunType = ReturnType!(typeof(cmdInst.run));
                                auto resAny = cmdInst.run(ctx);
                                auto res = cast(RunType) resAny;
                                if (!res.succeeded) {
                                    mcpLog("[MCP] command failed: %s", res.message);
                                }
                                return commandResultToJson!RunType(res);
                            }
                            return JSONValue(["status": JSONValue("skipped"), "succeeded": JSONValue(false), "message": JSONValue("Command not runnable")]);
                        });
                    }
                );
            }
        }
        // Summary per AA (useful to spot empty maps)
        mcpLog("[MCP] addTool summary: AA=%s key=%s added=%s", typeof(AA).stringof, _McpKeyTypeOfAA!(AA).stringof, _aaAdded);
    }}

    // Also expose as Resources (for clients expecting MCP resources)
    // Dynamic resource: resource://nijigenerate/resources/find?selector=...
    mcpLog("[MCP] addDynamicResource: %s", "resource://nijigenerate/resources/find?");
    gNotifyResourcesFind = server.addDynamicResource(
        "resource://nijigenerate/resources/find?",
        "Find Resources",
        "Find resources by selector. Returns JSON tree with Basics + Children.",
        (string path) {
            // path is the query string after '?', e.g. "selector=..." (URL-encoded)
            mcpLog("[MCP] read resource resources/find?%s", path);
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
            JSONValue result;

            result = ngRunInMainThread({
                SerializeNodeFlags flags = SerializeNodeFlags.Basics; // fixed
                Selector sel = new Selector();
                if (selectorParam.length) sel.build(selectorParam);
                auto results = sel.run();

                import nijigenerate.core.selector.treestore : TreeStore_, TreeStore;
                auto ts = new TreeStore_!false();
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

                return JSONValue(["items": JSONValue(rootsOut)]);
            });

            import mcp.resources : ResourceContents;
            return ResourceContents.makeText("application/json", result.toString());
        }
    );

    // Template resource: resource://nijigenerate/resources/{uuid}
    mcpLog("[MCP] addTemplate: %s", "resource://nijigenerate/resources/{uuid}");
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
            JSONValue result;

            result = ngRunInMainThread({
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
                return JSONValue(["item": obj]);
            });

            import mcp.resources : ResourceContents;
            return ResourceContents.makeText("application/json", result.toString());
        }
    );

    // Static resource: index of all UUID-based resource URIs for current puppet
    mcpLog("[MCP] addResource: %s", "resource://nijigenerate/resources/index");
    gNotifyResourceIndex = server.addResource(
        "resource://nijigenerate/resources/index",
        "Resource Index",
        "List of UUID-based resource URIs for current puppet.",
        () {
            // Build the index on the main thread to ensure the active puppet is visible.
            auto payload = ngRunInMainThread({
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
                return JSONValue(["items": JSONValue(items)]);
            });
            import mcp.resources : ResourceContents;
            return ResourceContents.makeText("application/json", payload.toString());
        }
    );

    auto t = new Thread({
        mcpLog("[MCP] server thread entering start() ...");
        server.start();
        mcpLog("[MCP] server thread exited start()");
    });
    gServerThread = t;
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
    mcpLog("[MCP] addPrompt: resources/find");
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

    mcpLog("[MCP] addPrompt: resources/get");
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

    mcpLog("[MCP] addPrompt: selectors/guide");
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

    mcpLog("[MCP] addPrompt: resources/guide");
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

    mcpLog("[MCP] addPrompt: tools/guide");
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

// Public: initialize and start MCP server in background
void ngMcpInit(string host = "127.0.0.1", ushort port = 8088) {
    _ngMcpStart(host, port);
}

// Public: stop MCP server if running
void ngMcpStop() {
    if (!gServerStarted) { mcpLog("[MCP] stop requested but server not started"); return; }
    gServerEnabled = false;
    if (gServerInstance !is null) {
        mcpLog("[MCP] requesting server stop...");
        try {
            gServerInstance.stop();
            mcpLog("[MCP] stop() invoked on transport");
            // Wait briefly for transport event loop to exit to free the port
            import core.time : msecs;
            import core.thread : Thread;
            foreach (i; 0 .. 20) { // up to ~1s
                if (gServerInstance.transportExited()) { mcpLog("[MCP] transport exited"); break; }
                Thread.sleep(50.msecs);
            }
        } catch (Exception e) {
            mcpLog("[MCP] stop() threw: %s", e.msg);
        }
    } else mcpLog("[MCP] no server instance to stop");
    // Do not block the UI thread waiting for shutdown; the server thread is daemon and will exit.
    gServerThread = null;
    gServerInstance = null;
    gServerStarted = false;
    mcpLog("[MCP] server state cleared (stopped=true)");
}

// Public: apply settings without per-frame polling
void ngMcpApplySettings(bool enabled, string host, ushort port) {
    mcpLog("[MCP] apply settings: enabled=%s host=%s port=%s (running=%s h=%s p=%s)", enabled, host, port, gServerStarted, gServerHost, gServerPort);
    if (!enabled) {
        if (gServerStarted) {
            mcpLog("[MCP] settings disabled -> stopping");
            ngMcpStop();
        } else {
            mcpLog("[MCP] settings disabled and not running -> no-op");
        }
        return;
    }
    if (!gServerStarted) {
        mcpLog("[MCP] settings enabled and not running -> start");
        _ngMcpStart(host, port);
        return;
    }
    if (host != gServerHost || port != gServerPort) {
        mcpLog("[MCP] settings changed host/port -> restart");
        ngMcpStop();
        _ngMcpStart(host, port);
    } else {
        mcpLog("[MCP] settings unchanged and running -> no-op");
    }
}

// Public: read settings and apply in one call (no per-frame polling)
void ngMcpLoadSettings() {
    // Default to disabled to avoid unexpected background server on first run
    bool enabled = incSettingsGet!bool("MCP.Enabled", false);
    string host = incSettingsGet!string("MCP.Host", "127.0.0.1");
    ushort port = cast(ushort) incSettingsGet!int("MCP.Port", 8088);
    mcpLog("[MCP] load settings: enabled=%s host=%s port=%s", enabled, host, port);
    ngMcpApplySettings(enabled, host, port);
}

void ngAcpLoadSettings() {
    // no-op: ACP settings are read on demand via incSettingsGet
}

void ngMcpAuthEnabled(bool value) {
    if (gTransport) gTransport.authEnabled = value;
}

bool ngMcpAuthEnabled() {
    if (gTransport) return gTransport.authEnabled;
    return false;
}
