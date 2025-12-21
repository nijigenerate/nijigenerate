module nijigenerate.api.acp.echo_agent;

version (ACP_ECHO_TOOL) {

import std.stdio;
import std.string;
import std.json;
import std.conv : to;

/**
 * Simple ACP echo agent for debugging.
 * - Reads JSON-RPC messages from stdin.
 * - On "initialize": replies with minimal capabilities.
 * - Otherwise: echoes params back in result.
 * Usage: ldc2 -O -release -version=ACP_ECHO_TOOL -of=out/acp-echo source/nijigenerate/api/acp/echo_agent.d
 * Transport: Content-Length framing (LSP/MCP format) or single-line JSON.
 */

void main() {
    while (!stdin.eof) {
        auto msg = readFramed();
        if (msg.length == 0) continue;
        JSONValue json;
        try {
            json = parseJSON(msg);
        } catch(Exception) {
            continue; // ignore invalid
        }
        auto response = handle(json);
        if (response.type != JSONType.null_) {
            writeFramed(response.toString());
        }
    }
}

private JSONValue handle(JSONValue req) {
    if (req.type != JSONType.object) return JSONValue(null);
    string method = req["method"].str;
    JSONValue id = JSONValue(null);
    if ("id" in req) id = req["id"];

    if (method == "initialize") {
        auto result = JSONValue([
            "protocolVersion": JSONValue("2024-11-05"),
            "serverInfo": JSONValue([
                "name": JSONValue("EchoAgent"),
                "version": JSONValue("0.0.1")
            ])
        ]);
        return makeResp(id, result);
    }

    JSONValue params = JSONValue(null);
    if ("params" in req) params = req["params"];
    auto result = JSONValue(["echo": params]);
    return makeResp(id, result);
}

private JSONValue makeResp(JSONValue id, JSONValue result) {
    JSONValue[string] obj;
    obj["jsonrpc"] = JSONValue("2.0");
    obj["id"] = id;
    obj["result"] = result;
    return JSONValue(obj);
}

private string readFramed() {
    auto line = stdin.readln();
    if (line.length == 0) return line;
    line = line.chomp();
    if (line.startsWith("Content-Length")) {
        auto parts = line.split(":");
        size_t len = (parts.length > 1) ? parts[1].strip.to!size_t : 0;
        stdin.readln(); // empty line
        auto buf = new char[](len);
        stdin.rawRead(buf);
        return cast(string)buf;
    }
    return line;
}

private void writeFramed(string payload) {
    writeln("Content-Length: ", payload.length);
    writeln();
    writeln(payload);
    stdout.flush();
}

} // version(ACP_ECHO_TOOL)
