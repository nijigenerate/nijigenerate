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
 *   - `armedParameters`: array of uint UUIDs for commands that explicitly operate on armed parameters
 *   - `nodes`: array of uint UUIDs for Nodes
 *   - `parameterValue`: [number] or [number, number] parameter-axis values resolved by the target command
 * - Any missing key is treated as "not set" for that value (hasXXX=false in Context).
 */

import core.thread : Thread;
import std.json;
import std.array : array, join, split;
import std.conv : to;
import std.algorithm : canFind;
import std.traits : isInstanceOf, TemplateArgsOf, isIntegral, isFloatingPoint, isSomeString, BaseClassesTuple, EnumMembers, ReturnType;
import std.stdio : writefln;

// nijigenerate command system
import nijigenerate.commands; // AllCommandMaps, Command, Context
import nijigenerate.commands.base : BaseExArgsOf, CommandResult, enrichArgDesc, ngCommandIdFromKey;
import nijigenerate.commands.automesh.config : AutoMeshTypedCommand; // for CT logs
import nijigenerate.project : incActivePuppet, incRegisterLoadFunc;
import nijilive; // Node, Parameter, Puppet
import nijilive.core.param.binding : ParameterBinding, ValueParameterBinding, DeformationParameterBinding, ParameterParameterBinding;
import nijigenerate.ext.param : ExParameterGroup;

// mcp-d
import mcp.server;
import mcp.schema : SchemaBuilder;
import mcp.prompts : PromptArgument, PromptResponse, PromptMessage; // proper prompt API
import mcp.resources : ResourceNotifier, ResourceContents; // for resource change notifications
import nijigenerate.api.mcp.http_transport;
import nijigenerate.utils.crashdump : installNativeCrashDumpThreadHandler;
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
import nijigenerate.api.mcp.helpers : commandResultToJsonRuntime, buildContextFromPayload, applyPayloadToInstance;

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

private JSONValue _mcpUnwrapDirectToolResult(JSONValue resultJson) {
    if (resultJson.type == JSONType.object && "result" in resultJson) {
        auto direct = resultJson["result"];
        if (direct.type == JSONType.object && "mcpDirectToolResult" in direct && "content" in direct) {
            direct.object.remove("mcpDirectToolResult");
            return direct;
        }
    }
    return resultJson;
}

private CommandResult _mcpRunCommandInstance(C)(C inst, Context ctx, string toolName) if (is(C : Command)) {
    auto res = inst.run(ctx);
    return res;
}

private JSONValue _mcpEncodeCommandResult(CommandResult res, string toolName) {
    res = res.waitForCompletion();
    if (!res.succeeded) {
        mcpLog("[MCP] command failed: %s", res.message);
    }
    auto resultJson = commandResultToJsonRuntime(res);
    return _mcpUnwrapDirectToolResult(resultJson);
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
private __gshared ResourceNotifier gNotifyBindingByDescriptor;
private __gshared bool gLoadHookRegistered = false;

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

    void addConcreteResource(uint uuid, string name, string typeId) {
        auto uri = "resource://nijigenerate/resources/" ~ to!string(uuid);
        mcpLog("[MCP] addResource: %s", uri);
        server.addResource(
            uri,
            name,
            typeId ~ " resource instance.",
            () {
                JSONValue result = ngRunInMainThread({
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
                return ResourceContents.makeText("application/json", result.toString());
            }
        );
    }

    void addGuideResource(string uri, string name, string description, string content) {
        mcpLog("[MCP] addResource: %s", uri);
        server.addResource(
            uri,
            name,
            description,
            () {
                import mcp.resources : ResourceContents;
                return ResourceContents.makeText("text/markdown", content);
            }
        );
    }

    string bindingResourceUri(Parameter param, ParameterBinding binding) {
        import std.uri : encodeComponent;
        auto target = binding.getTarget().target;
        uint targetUuid = target !is null ? target.uuid : binding.getNodeUUID();
        return "resource://nijigenerate/bindings/get?parameter=" ~ to!string(param.uuid) ~
            "&target=" ~ to!string(targetUuid) ~
            "&name=" ~ encodeComponent(binding.getTarget().name);
    }

    bool parseBindingDescriptorPath(string path, out uint parameterUuid, out uint targetUuid, out string bindingName, out string message) {
        import std.uri : decodeComponent;
        string[string] query;
        foreach (part; ("parameter=" ~ path).split("&")) {
            if (part.length == 0) continue;
            auto pieces = part.split("=");
            if (pieces.length < 2) continue;
            query[decodeComponent(pieces[0])] = decodeComponent(pieces[1 .. $].join("="));
        }
        if ("parameter" !in query || "target" !in query || "name" !in query) {
            message = "Binding read requires parameter, target, and name";
            return false;
        }
        try {
            parameterUuid = to!uint(query["parameter"]);
            targetUuid = to!uint(query["target"]);
            bindingName = query["name"];
        } catch (Exception e) {
            message = "Binding read parameter and target must be integer UUIDs";
            return false;
        }
        if (bindingName.length == 0) {
            message = "Binding read name must not be empty";
            return false;
        }
        return true;
    }

    JSONValue bindingToJson(Parameter param, ParameterBinding binding) {
        JSONValue[string] map;
        auto target = binding.getTarget().target;
        uint targetUuid = target !is null ? target.uuid : binding.getNodeUUID();
        string targetTypeId = "";
        if (auto node = cast(Node)target) targetTypeId = node.typeId;
        else if (cast(Parameter)target) targetTypeId = "Parameter";

        map["typeId"] = JSONValue("Binding");
        map["parameter"] = JSONValue([
            "uuid": JSONValue(cast(long)param.uuid),
            "name": JSONValue(param.name)
        ]);
        map["target"] = JSONValue([
            "uuid": JSONValue(cast(long)targetUuid),
            "name": JSONValue(target !is null ? target.name : ""),
            "typeId": JSONValue(targetTypeId)
        ]);
        map["name"] = JSONValue(binding.getTarget().name);
        map["interpolateMode"] = JSONValue(binding.interpolateMode().to!string);
        map["setCount"] = JSONValue(cast(long)binding.getSetCount());
        map["uri"] = JSONValue(bindingResourceUri(param, binding));
        JSONValue[] axisOffsets;
        JSONValue[] axisValues;
        foreach (axis; 0 .. 2) {
            JSONValue[] offsets;
            JSONValue[] values;
            foreach (offset; param.axisPoints[axis]) {
                offsets ~= JSONValue(cast(double)offset);
                values ~= JSONValue(cast(double)param.unmapAxis(cast(uint)axis, offset));
            }
            axisOffsets ~= JSONValue(offsets);
            axisValues ~= JSONValue(values);
        }
        map["axisOffsets"] = JSONValue(axisOffsets);
        map["axisValues"] = JSONValue(axisValues);

        auto app = appender!(char[]);
        auto ser = inCreateSerializer(app);
        binding.serializeSelf(ser);
        ser.flush();
        map["data"] = parseJSON(cast(string)app.data);
        return JSONValue(map);
    }

    Parameter bindingOwnerParameter(ParameterBinding binding) {
        if (auto b = cast(ValueParameterBinding)binding) return b.parameter;
        if (auto b = cast(DeformationParameterBinding)binding) return b.parameter;
        if (auto b = cast(ParameterParameterBinding)binding) return b.parameter;
        return null;
    }

    JSONValue readBindingByDescriptor(uint parameterUuid, uint targetUuid, string bindingName) {
        JSONValue[string] result;
        auto puppet = incActivePuppet();
        if (puppet is null) {
            result["item"] = JSONValue(null);
            result["error"] = JSONValue("No active puppet");
            return JSONValue(result);
        }

        auto param = puppet.find!(Parameter)(parameterUuid);
        if (param is null) {
            result["item"] = JSONValue(null);
            result["error"] = JSONValue("Parameter not found: " ~ parameterUuid.to!string);
            return JSONValue(result);
        }

        import nijilive.core.resource : LiveResource = Resource;
        LiveResource target = puppet.find!(Node)(targetUuid);
        if (target is null) target = puppet.find!(Parameter)(targetUuid);
        if (target is null) {
            result["item"] = JSONValue(null);
            result["error"] = JSONValue("Target not found: " ~ targetUuid.to!string);
            return JSONValue(result);
        }

        auto binding = param.getBinding(target, bindingName);
        if (binding is null) {
            result["item"] = JSONValue(null);
            result["error"] = JSONValue("Binding not found: parameter=" ~ parameterUuid.to!string ~
                ", target=" ~ targetUuid.to!string ~ ", name=" ~ bindingName);
            return JSONValue(result);
        }

        result["item"] = bindingToJson(param, binding);
        return JSONValue(result);
    }

    ResourceContents buildFindResource(string selectorParam) {
        JSONValue result = ngRunInMainThread({
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
                map["uri"] = JSONValue("resource://nijigenerate/resources/" ~ to!string(res.uuid));

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
                if (auto binding = nijigenerate.core.selector.resource.to!ParameterBinding(res)) {
                    Parameter param = null;
                    auto source = res.source;
                    while (source !is null) {
                        param = nijigenerate.core.selector.resource.to!Parameter(source);
                        if (param !is null) break;
                        source = source.source;
                    }
                    if (param is null) param = bindingOwnerParameter(binding);
                    if (param !is null) {
                        map["uri"] = JSONValue(bindingResourceUri(param, binding));
                        map["parameter"] = JSONValue([
                            "uuid": JSONValue(cast(long)param.uuid),
                            "name": JSONValue(param.name)
                        ]);
                        auto target = binding.getTarget().target;
                        uint targetUuid = target !is null ? target.uuid : binding.getNodeUUID();
                        string targetTypeId = "";
                        if (auto targetNode = cast(Node)target) targetTypeId = targetNode.typeId;
                        else if (cast(Parameter)target) targetTypeId = "Parameter";
                        map["target"] = JSONValue([
                            "uuid": JSONValue(cast(long)targetUuid),
                            "name": JSONValue(target !is null ? target.name : ""),
                            "typeId": JSONValue(targetTypeId)
                        ]);
                        map["bindingName"] = JSONValue(binding.getTarget().name);
                    }
                }

                JSONValue[] childArr;
                if (res in ts.children) foreach (child; ts.children[res]) childArr ~= makeTree(child);
                map["children"] = JSONValue(childArr);
                return JSONValue(map);
            }

            JSONValue[] rootsOut;
            foreach (r; ts.roots) rootsOut ~= makeTree(r);

            return JSONValue([
                "items": JSONValue(rootsOut),
                "selectorSyntax": JSONValue("nijigenerate.core.selector"),
                "recommendedDiscoveryUri": JSONValue("resource://nijigenerate/resources/find?selector=*"),
                "guideUris": JSONValue([
                    JSONValue("resource://nijigenerate/guides/find"),
                    JSONValue("resource://nijigenerate/guides/selectors"),
                    JSONValue("resource://nijigenerate/guides/resources")
                ])
            ]);
        });

        return ResourceContents.makeText("application/json", result.toString());
    }

    // Minimal, robust registration that does not depend on TLS of another thread
    bool[string] registered;
    static foreach (AA; AllCommandMaps) {{
        size_t _aaAdded = 0;
        foreach (k, v; AA) {
            if (!v.mcpExposed()) continue;
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
            string[] baseNames = [newBaseName];

            auto contextSchema = SchemaBuilder.object()
                .setDescription(
                    "Optional execution context. Omit this property to use active app state. " ~
                    "Use parameterValue instead of internal keyPoint indexes.")
                .addProperty("nodes", SchemaBuilder.array(SchemaBuilder.integer()).optional()
                    .setDescription("Node UUIDs to use as context.nodes. Obtain UUIDs from resources/find or resources/{uuid}."))
                .addProperty("parameters", SchemaBuilder.array(SchemaBuilder.integer()).optional()
                    .setDescription("Parameter UUIDs to use as context.parameters."))
                .addProperty("armedParameters", SchemaBuilder.array(SchemaBuilder.integer()).optional()
                    .setDescription("Parameter UUIDs for commands that explicitly use armed-parameter context. Only arm-specific commands change the GUI ArmedParameter."))
                .addProperty("parameterValue", SchemaBuilder.array(SchemaBuilder.number()).optional()
                    .setDescription("Parameter-axis key values, not keypoint indexes. Use [x] for 1D or [x,y] for 2D. Values must exactly match existing parameter key values."))
                .addProperty("bindings", SchemaBuilder.array(
                    SchemaBuilder.object()
                        .addProperty("target", SchemaBuilder.integer()
                            .setDescription("UUID of the binding target Node or Parameter."))
                        .addProperty("name", SchemaBuilder.string_()
                            .setDescription("Binding name on the target, such as deform or transform.t.x."))
                ).optional()
                    .setDescription("Binding descriptors for commands such as RemoveBinding and SetInterpolation. Requires context.parameters[0]. Bindings are resolved by parameter + target UUID + binding name; do not use Binding pseudo-UUIDs."))
                .allowAdditional(true);

            // Build tool input schema (command args plus common context).
            auto inputSchema = SchemaBuilder.object()
                .setDescription("Input for nijigenerate Command tool. Omit context to use active app state; other parameters map to command-specific arguments.")
                .addProperty("context", contextSchema.optional())
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
                                } else static if (is(TParam == enum)) {
                                    string[] enumNames;
                                    static foreach (mem; EnumMembers!TParam) {{
                                        enumNames ~= __traits(identifier, mem);
                                        static if (__traits(compiles, cast(string)mem)) {
                                            enum string memValue = cast(string)mem;
                                            static if (memValue != __traits(identifier, mem)) {
                                                enumNames ~= memValue;
                                            }
                                        }
                                    }}
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.enum_(enumNames).setDescription(desc));
                                    paramLog ~= fname ~ ":enum";
                                } else static if (isIntegral!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.integer().setDescription(desc));
                                    paramLog ~= fname ~ ":int";
                                } else static if (isFloatingPoint!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.number().setDescription(desc));
                                    paramLog ~= fname ~ ":number";
                                } else static if (isSomeString!TParam) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.string_().setDescription(desc));
                                    paramLog ~= fname ~ ":string";
                                } else static if (is(TParam == string[])) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(SchemaBuilder.string_()).setDescription((desc.length?desc~"; ":"")~"string[]"));
                                    paramLog ~= fname ~ ":string[]";
                                } else static if (is(TParam == JSONValue)) {
                                    inputSchema = inputSchema.addProperty(fname, SchemaBuilder.array(
                                        SchemaBuilder.object()
                                            .addProperty("uuid", SchemaBuilder.integer()
                                                .setDescription("Target Node UUID."))
                                            .addProperty("overlay", SchemaBuilder.enum_(["bounds", "mesh"]).optional()
                                                .setDescription("Overlay kind. Use bounds or mesh."))
                                            .addProperty("type", SchemaBuilder.enum_(["bounds", "mesh"]).optional()
                                                .setDescription("Alias of overlay."))
                                    ).optional().setDescription(desc));
                                    paramLog ~= fname ~ ":json";
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
                    "Action: " ~ (toolDesc.length ? toolDesc : ("Run command " ~ toolNameLocal)) ~
                    "\n\nThis endpoint may mutate app state or perform side effects." ~
                    "\n\nInput: optional 'context' object. Omit context to use active app state. Use context.parameterValue for parameter-axis key values.",
                    inputSchema,
                    (JSONValue payload) {
                        // Debug: print incoming JSON for this tool call
                        mcpLog("[MCP] call %s: %s", toolNameLocal, payload.toString());
                        auto payloadCopy = payload;
                        auto commandResult = ngRunInMainThread!CommandResult({
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
                                CommandResult concreteResult;
                                bool concreteHandled = false;
                                static if (is(K == enum)) static foreach (m; EnumMembers!K) {{
                                    if (k == m) {{
                                        enum _mName  = __traits(identifier, m);
                                        enum _typeName = _mName ~ "Command";
                                        static if (__traits(compiles, mixin(_typeName))) {
                                            alias C = mixin(_typeName);
                                            if (auto inst = cast(C) cmdInst) {
                                                concreteResult = _mcpRunCommandInstance(inst, ctx, toolNameLocal);
                                                concreteHandled = true;
                                            }
                                        }
                                    }}
                                }}
                                if (concreteHandled) return concreteResult;
                                return _mcpRunCommandInstance(cmdInst, ctx, toolNameLocal);
                            }
                            return CommandResult(false, "Command not runnable");
                        });
                        return _mcpEncodeCommandResult(commandResult, toolNameLocal);
                    }
                );
            }
        }
        // Summary per AA (useful to spot empty maps)
        mcpLog("[MCP] addTool summary: AA=%s key=%s added=%s", typeof(AA).stringof, _McpKeyTypeOfAA!(AA).stringof, _aaAdded);
    }}

    // Also expose as Resources (for clients expecting MCP resources)
    enum string GUIDES_RESOURCES =
        "# Resources Guide\n\n" ~
        "- Broad discovery: read `resource://nijigenerate/resources/find?selector=*`\n" ~
        "- Narrow discovery: use `resource://nijigenerate/resources/find?selector=...`\n" ~
        "- Read one item: use `resource://nijigenerate/resources/{uuid}`\n" ~
        "- Selector syntax: read `resource://nijigenerate/guides/selectors`\n" ~
        "- Find endpoint usage: read `resource://nijigenerate/guides/find`\n";

    enum string GUIDES_SELECTORS =
        "# Selectors Guide\n\n" ~
        "Used by `resource://nijigenerate/resources/find?selector=...`\n\n" ~
        "Basics:\n" ~
        "- `*` any resource\n" ~
        "- `Node`, `Part`, `Parameter`, `Binding`, `Group`\n" ~
        "- `.name` match by name\n" ~
        "- `#123` match by UUID\n" ~
        "- `[name=\"Eye\"]` attribute match\n" ~
        "- `A > B` child combinator\n" ~
        "- `A B` descendant combinator\n" ~
        "- `A, B` union\n\n" ~
        "Recommended first query:\n" ~
        "- `resource://nijigenerate/resources/find?selector=*`\n";

    enum string GUIDES_FIND =
        "# Find Guide\n\n" ~
        "Endpoint: `resource://nijigenerate/resources/find?selector=...`\n\n" ~
        "Recommended first query:\n" ~
        "- `resource://nijigenerate/resources/find?selector=*`\n\n" ~
        "Examples:\n" ~
        "- all nodes: `resource://nijigenerate/resources/find?selector=Node`\n" ~
        "- direct child parts: `resource://nijigenerate/resources/find?selector=Node%20%3E%20Part`\n" ~
        "- part named Eye: `resource://nijigenerate/resources/find?selector=Part.%22Eye%22`\n\n" ~
        "Next step:\n" ~
        "- follow each item's `uri` with `resources/read`\n";

    addGuideResource(
        "resource://nijigenerate/guides/resources",
        "Resources Guide",
        "Guide for resource discovery and reading.",
        GUIDES_RESOURCES
    );
    addGuideResource(
        "resource://nijigenerate/guides/selectors",
        "Selectors Guide",
        "Selector syntax reference for resources/find.",
        GUIDES_SELECTORS
    );
    addGuideResource(
        "resource://nijigenerate/guides/find",
        "Find Guide",
        "Usage examples for resources/find.",
        GUIDES_FIND
    );

    // Dynamic resource: resource://nijigenerate/resources/find?selector=...
    mcpLog("[MCP] addDynamicResource: %s", "resource://nijigenerate/resources/find?selector=");
    gNotifyResourcesFind = server.addDynamicResource(
        "resource://nijigenerate/resources/find?selector=",
        "Start Here: Explore Entire Model Tree",
        "Hierarchical resource discovery by selector.",
        (string path) {
            // path is the URL-encoded selector after '?selector='.
            mcpLog("[MCP] read resource resources/find?selector=%s", path);
            import std.uri : decodeComponent;
            string selectorParam = decodeComponent(path);
            return buildFindResource(selectorParam);
        }
    );

    // Dynamic resource: resource://nijigenerate/bindings/get?parameter=...&target=...&name=...
    mcpLog("[MCP] addDynamicResource: %s", "resource://nijigenerate/bindings/get?parameter=");
    gNotifyBindingByDescriptor = server.addDynamicResource(
        "resource://nijigenerate/bindings/get?parameter=",
        "Read Binding By Descriptor",
        "Read binding values by parameter UUID, target UUID, and binding name.",
        (string path) {
            uint parameterUuid;
            uint targetUuid;
            string bindingName;
            string message;
            JSONValue result = ngRunInMainThread({
                if (!parseBindingDescriptorPath(path, parameterUuid, targetUuid, bindingName, message))
                    return JSONValue(["item": JSONValue(null), "error": JSONValue(message)]);
                return readBindingByDescriptor(parameterUuid, targetUuid, bindingName);
            });
            return ResourceContents.makeText("application/json", result.toString());
        }
    );

    // Register concrete resources so resources/list enumerates the current puppet instances.
    auto puppetForList = incActivePuppet();
    if (puppetForList !is null) {
        bool[uint] seen;

        void addNodeResource(Node n) {
            if (n is null) return;
            if (n.uuid in seen) return;
            seen[n.uuid] = true;
            addConcreteResource(n.uuid, n.name, n.typeId);
            foreach (c; n.children) addNodeResource(c);
        }

        addNodeResource(puppetForList.root);

        foreach (param; puppetForList.parameters) {
            if (param is null) continue;
            if (param.uuid in seen) continue;
            seen[param.uuid] = true;
            addConcreteResource(param.uuid, param.name, "Parameter");
        }
    }

    // Template resource: resource://nijigenerate/resources/{uuid}
    mcpLog("[MCP] addTemplate: %s", "resource://nijigenerate/resources/{uuid}");
    gNotifyResourceByUuid = server.addTemplate(
        "resource://nijigenerate/resources/{uuid}",
        "Read Resource Instance By UUID",
        "Read one resource instance by UUID.",
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

    auto t = new Thread({
        installNativeCrashDumpThreadHandler();
        mcpLog("[MCP] server thread entering start() ...");
        server.start();
        mcpLog("[MCP] server thread exited start()");
    });
    gServerThread = t;
    t.isDaemon = true;
    t.start();

    // Notify MCP clients that resources have changed when a project is loaded.
    if (!gLoadHookRegistered) {
        gLoadHookRegistered = true;
        incRegisterLoadFunc((puppet) {
            if (gNotifyResourcesFind) gNotifyResourcesFind();
            if (gNotifyResourceByUuid) gNotifyResourceByUuid();
            if (gNotifyBindingByDescriptor) gNotifyBindingByDescriptor();
            mcpLog("[MCP] puppet load detected -> resources changed");
        });
    }

    // Register helpful prompts (best-effort; only if SDK supports prompts)
    enum string RES_FIND_PROMPT =
        "Resource Endpoint: resources/find\n" ~
        "Purpose: Hierarchical exploration ONLY. Use selector to locate resource instances and their UUIDs. For reading content, use resources/{uuid}. Returns Basics + Children tree.\n\n" ~
        "Recommended first step:\n" ~
        "- Start broad discovery with resource://nijigenerate/resources/find?selector=*\n\n" ~
        "Selector syntax guide:\n" ~
        "- See resource: resource://nijigenerate/guides/selectors\n\n" ~
        "Workflow guide:\n" ~
        "- See resource: resource://nijigenerate/guides/resources\n\n" ~
        "Input fields:\n" ~
        "- selector (string): nijigenerate.core.selector query\n\n" ~
        "Examples:\n" ~
        "1) List all nodes (basic info)\n" ~
        "   resource://nijigenerate/resources/find?selector=Node\n" ~
        "2) By name (Part named \"Eye\")\n" ~
        "   resource://nijigenerate/resources/find?selector=Part.%22Eye%22\n" ~
        "3) Direct children Parts of any Node\n" ~
        "   resource://nijigenerate/resources/find?selector=Node%20%3E%20Part\n" ~
        "4) Active bindings of selected node\n" ~
        "   resource://nijigenerate/resources/find?selector=Binding%3Aactive\n\n" ~
        "MCP command binding input:\n" ~
        "- Do not pass Binding pseudo-UUIDs to tools. For commands such as RemoveBinding, pass context.parameters[0] plus context.bindings=[{target:<Node-or-Parameter UUID>, name:<binding name>}].\n\n" ~
        "Binding value read endpoint:\n" ~
        "- resource://nijigenerate/bindings/get?parameter=<parameter UUID>&target=<target UUID>&name=<binding name>\n\n" ~
        "Official read endpoint:\n" ~
        "- resource://nijigenerate/resources/{uuid} (see resources/get)\n";

    enum string RESOURCE_PROMPT =
        "Resource Endpoint: resources/get\n" ~
        "Purpose: OFFICIAL read endpoint. Fetch one resource instance by UUID. Node returns Basics + State + Geometry + Links; Parameter returns Basics.\n\n" ~
        "Binding values use the descriptor endpoint instead of UUID:\n" ~
        "- resource://nijigenerate/bindings/get?parameter=<parameter UUID>&target=<target UUID>&name=<binding name>\n\n" ~
        "Input fields:\n" ~
        "- uuid (integer): Numeric UUID of a Node or Parameter.\n\n" ~
        "Examples:\n" ~
        "- Get a node by UUID\n" ~
        "  uuid=123456\n";

    enum string SELECTOR_GUIDE =
        "Selector Syntax (nijigenerate.core.selector)\n\n" ~
        "Used by:\n" ~
        "- resource://nijigenerate/resources/find?selector=...\n\n" ~
        "Recommended discovery query:\n" ~
        "- resource://nijigenerate/resources/find?selector=*\n\n" ~
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
        "Tool input:\n" ~
        "- Binding resources are for inspection. MCP tools identify bindings as {target:<UUID>, name:<binding name>} under context.bindings, together with context.parameters[0].\n\n" ~
        "Binding value read:\n" ~
        "- Use the Binding item's URI: resource://nijigenerate/bindings/get?parameter=<parameter UUID>&target=<target UUID>&name=<binding name>\n\n" ~
        "Combinator examples:\n" ~
        "- Node > Part        → direct Part children of any Node.\n" ~
        "- Node Part          → any descendant Part under any Node.\n" ~
        "- Node, Part         → all Nodes and Parts.\n\n" ~
        "Result shape:\n" ~
        "- resources/find returns a tree under 'items'.\n" ~
        "- Each returned node includes 'uri' so clients can follow up with resources/read.\n" ~
        "- Child combinators and descendant queries preserve hierarchy in the returned tree.\n\n" ~
        "Advanced examples:\n" ~
        "- Part.#123          → Part with UUID 123.\n" ~
        "- Part[name=\"Eye\"] → Part whose name is \"Eye\".\n" ~
        "- Binding:active     → only active bindings for the armed parameter.\n\n" ~
        "Tips:\n" ~
        "- Start with selector=* when you need the broad current resource tree.\n" ~
        "- Prefer #UUID for precise targeting.\n" ~
        "- Build lists with resources/find, then fetch details by UUID.\n";

    enum string RESOURCES_GUIDE =
        "Resources vs Tools (Official Policy)\n\n" ~
        "Policy:\n" ~
        "- Exploration: Use resources/find (hierarchical traversal only).\n" ~
        "- Reading:    Use resources/{uuid} (official endpoint) to fetch content.\n" ~
        "- Listing:    'resources/list' returns currently readable resource instance URIs for the active puppet.\n" ~
        "- Tools:      Use Tool commands only to mutate app state or perform actions.\n\n" ~
        "URIs:\n" ~
        "- resource://nijigenerate/resources/find?selector=... (exploration)\n" ~
        "- resource://nijigenerate/resources/{uuid} (read one resource instance)\n" ~
        "- resource://nijigenerate/bindings/get?parameter=...&target=...&name=... (read one binding value grid)\n" ~
        "- resources/list (list current resource instances)\n\n" ~
        "Prompt links:\n" ~
        "- resource://nijigenerate/guides/find describes the exploration endpoint and recommends selector=* for first-pass discovery.\n" ~
        "- resource://nijigenerate/guides/selectors describes selector syntax and returned tree shape.\n" ~
        "- resource://nijigenerate/guides/resources describes the overall exploration/read workflow.\n\n" ~
        "Typical flow:\n" ~
        "1) Start broad discovery with resources/find?selector=*.\n" ~
        "2) Narrow with more specific selectors as needed.\n" ~
        "3) Read details via resources/{uuid}.\n" ~
        "4) Read binding values via bindings/get descriptor URIs when needed.\n" ~
        "5) Optionally call Tools to modify.\n\n" ~
        "Notes:\n" ~
        "- In nijigenerate, direct resource entries are rebuilt when a puppet is loaded so that 'resources/list' reflects current instances.\n" ~
        "- 'resources/templates/list' lists parameterized read definitions such as resources/{uuid}.\n" ~
        "- See also: 'selectors/guide' and 'tools/guide'.";

    enum string TOOLS_GUIDE =
        "Tools Input & Context\n\n" ~
        "Role:\n" ~
        "- Tools are action endpoints. They may mutate app state or perform side effects.\n" ~
        "- Use resources/list, resources/find, and resources/{uuid} for discovery and reading.\n\n" ~
        "Context (recommended null):\n" ~
        "- Omit context to use the active app state (same as shortcuts).\n" ~
        "- Provide only when needed to override selection.\n\n" ~
        "Context keys:\n" ~
        "- parameters: uint[] of Parameter UUIDs.\n" ~
        "- armedParameters: uint[] for commands that explicitly use armed-parameter context. Only arm-specific commands change the GUI ArmedParameter.\n" ~
        "- nodes: uint[] of Node UUIDs.\n" ~
        "- parameterValue: [x] or [x,y] parameter-axis values. Values must exactly match existing key values.\n" ~
        "- keyPoint is not part of the MCP input surface. Do not pass key point indexes through MCP.\n\n" ~
        "Other parameters:\n" ~
        "- Unless a tool defines additional properties, only 'context' is accepted in this build.\n" ~
        "- UUIDs are numeric (decimal). Obtain them via resources/find or resources/{uuid}.\n\n" ~
        "Examples (JSON args):\n" ~
        "- Run with default context: {}\n" ~
        "- Run for specific nodes: {\"context\": {\"nodes\":[123,456]}}\n" ~
        "- Set a parameter key without arming: {\"context\": {\"parameters\":[789], \"parameterValue\":[-0.5,0]}}\n" ~
        "- Set and arm a parameter key: {\"context\": {\"armedParameters\":[789], \"parameterValue\":[-0.5,0]}}\n";

    // Register prompts using the proper API (no static-if fallbacks)
    mcpLog("[MCP] addPrompt: resources/find");
    server.addPrompt(
        "resources/find",
        "How to use the resources/find resource endpoint",
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
        "Selector syntax, examples, and returned tree shape for resources/find",
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
            // Wait briefly for the transport/event loop to exit and the thread to finish.
            import core.time : msecs;
            import core.thread : Thread;
            foreach (i; 0 .. 40) { // up to ~2s
                bool exited = gServerInstance.transportExited();
                bool running = (gServerThread !is null) && gServerThread.isRunning();
                if (exited && !running) { mcpLog("[MCP] transport exited and thread stopped"); break; }
                Thread.sleep(50.msecs);
            }
        } catch (Exception e) {
            mcpLog("[MCP] stop() threw: %s", e.msg);
        }
    } else mcpLog("[MCP] no server instance to stop");

    // Only clear global references once the transport has exited and the thread has stopped.
    // Clearing early can leave a running thread without a strongly-held owner reference.
    bool canClear = true;
    if (gServerInstance !is null && !gServerInstance.transportExited()) canClear = false;
    if (gServerThread !is null && gServerThread.isRunning()) canClear = false;

    if (canClear) {
        if (gServerThread !is null) {
            try gServerThread.join(); catch (Exception) {}
        }
        gServerThread = null;
        gServerInstance = null;
        gTransport = null;
        gServerStarted = false;
        mcpLog("[MCP] server state cleared (stopped=true)");
    } else {
        mcpLog("[MCP][WARN] stop requested but server did not exit in time; keeping references");
    }
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
