module njc.commands;

import std.exception : enforce;
import std.json : JSONType, JSONValue, parseJSON;
import std.stdio : writefln, writeln;
import std.string : startsWith, strip;
import std.uri : encodeComponent;

import njc.config : Options;
import njc.http_transport : HttpTransport;
import njc.jsonrpc : escapeJson, rpcRequest;

int runCommand(Options options) {
    auto transport = new HttpTransport(options.endpoint);
    long nextId = 1;
    initialize(transport, nextId);
    auto response = dispatch(transport, nextId, options.command);
    printResponse(options, response);
    return 0;
}

private auto dispatch(HttpTransport transport, ref long nextId, string[] args) {
    enforce(args.length > 0, "missing command");

    if (args[0] == "tools") {
        enforce(args.length >= 2, "tools requires list or call");
        if (args[1] == "list") {
            enforce(args.length == 2, "tools list takes no arguments");
            return call(transport, nextId, "tools/list");
        }
        if (args[1] == "call") {
            enforce(args.length >= 3, "tools call requires a tool name");
            auto params = `{"name":"` ~ escapeJson(args[2]) ~ `","arguments":` ~ parseJsonOption(args[3 .. $], "{}") ~ `}`;
            return call(transport, nextId, "tools/call", params);
        }
    }

    if (args[0] == "resources") {
        enforce(args.length >= 2, "resources requires list, templates, or read");
        if (args[1] == "list") {
            enforce(args.length == 2, "resources list takes no arguments");
            return call(transport, nextId, "resources/list");
        }
        if (args[1] == "templates") {
            enforce(args.length == 2, "resources templates takes no arguments");
            return call(transport, nextId, "resources/templates/list");
        }
        if (args[1] == "read") {
            enforce(args.length == 3, "resources read requires a URI");
            return readUri(transport, nextId, args[2]);
        }
    }

    if (args[0] == "find") {
        enforce(args.length == 2, "find requires one selector");
        auto uri = "resource://nijigenerate/resources/find?selector=" ~ encodeComponent(args[1]);
        return readUri(transport, nextId, uri);
    }

    if (args[0] == "read") {
        enforce(args.length == 2, "read requires a resource UUID or URI");
        auto uri = args[1];
        if (!uri.startsWith("resource://")) {
            uri = "resource://nijigenerate/resources/" ~ encodeComponent(uri);
        }
        return readUri(transport, nextId, uri);
    }

    if (args[0] == "rpc") {
        enforce(args.length >= 2, "rpc requires a method");
        auto params = parseJsonOption(args[2 .. $], "{}");
        return call(transport, nextId, args[1], params);
    }

    throw new Exception("unknown command: " ~ args[0]);
}

private void initialize(HttpTransport transport, ref long nextId) {
    auto response = call(transport, nextId, "initialize",
        `{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"njc","version":"0.1.0"}}`);
    enforce("result" in response.object, "initialize response has no result");
}

private JSONValue call(HttpTransport transport, ref long nextId, string method, string paramsJson = "{}") {
    auto response = transport.postJson(rpcRequest(nextId++, method, paramsJson));
    enforce(response.type == JSONType.object, "MCP response is not a JSON object");
    if ("error" in response.object) {
        throw new Exception("MCP error: " ~ response["error"].toString());
    }
    return response;
}

private auto readUri(HttpTransport transport, ref long nextId, string uri) {
    auto params = `{"uri":"` ~ escapeJson(uri) ~ `"}`;
    return call(transport, nextId, "resources/read", params);
}

private string parseJsonOption(string[] args, string defaultJson) {
    if (args.length == 0) return defaultJson;
    enforce(args.length == 2 && args[0] == "--json", "expected --json <json>");
    auto parsed = parseJSON(args[1]);
    return parsed.toString();
}

private void printResponse(Options options, JSONValue response) {
    if (!options.summary || options.rawJson || options.command.length == 0) {
        writeln(response.toString());
        return;
    }

    if (options.command.length >= 2 && options.command[0] == "resources" && options.command[1] == "list") {
        printResourceList(response);
        return;
    }
    if (options.command.length >= 2 && options.command[0] == "tools" && options.command[1] == "list") {
        printToolList(response);
        return;
    }
    if (options.command.length >= 2 && options.command[0] == "resources" && options.command[1] == "templates") {
        printTemplateList(response);
        return;
    }

    writeln(response.toString());
}

private void printResourceList(JSONValue response) {
    auto resources = response["result"]["resources"].array;
    writefln("resources: %s", resources.length);
    foreach (res; resources) {
        auto name = res["name"].str;
        auto uri = res["uri"].str;
        auto desc = ("description" in res.object) ? res["description"].str : "";
        writefln("- %s", name);
        writefln("  uri: %s", uri);
        if (desc.length) writefln("  %s", firstLine(desc));
    }
}

private void printToolList(JSONValue response) {
    auto tools = response["result"]["tools"].array;
    writefln("tools: %s", tools.length);
    foreach (tool; tools) {
        auto name = tool["name"].str;
        auto desc = ("description" in tool.object) ? tool["description"].str : "";
        writefln("- %s", name);
        if (desc.length) writefln("  %s", firstLine(desc));
    }
}

private void printTemplateList(JSONValue response) {
    auto templates = response["result"]["resourceTemplates"].array;
    writefln("resource templates: %s", templates.length);
    foreach (tmpl; templates) {
        writefln("- %s", tmpl["name"].str);
        writefln("  uriTemplate: %s", tmpl["uriTemplate"].str);
        if ("description" in tmpl.object) writefln("  %s", firstLine(tmpl["description"].str));
    }
}

private string firstLine(string value) {
    foreach (i, ch; value) {
        if (ch == '\n' || ch == '\r') return value[0 .. i].strip;
    }
    return value.strip;
}
