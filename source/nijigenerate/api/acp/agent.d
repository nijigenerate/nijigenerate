module nijigenerate.api.acp.agent;

import std.json;
import std.algorithm : startsWith;
import std.conv : to;
import std.json : parseJSON;

import mcp.protocol : Request, Response;
import mcp.protocol : ErrorCode;

import nijigenerate.api.acp.protocol;
import nijigenerate.api.acp.types;
import nijigenerate.api.acp.transport.stdio;

alias ApplyEditHandler = bool delegate(WorkspaceEdit);

/// Minimal CodingAgent-side ACP server.
class ACPAgent {
    private {
        Transport transport;
        string name;
        string version_;
        bool initialized;
    }

    /// Optional handler invoked when editor requests workspace/applyEdit.
    ApplyEditHandler onApplyEdit;

    this(Transport transport, string name = "nijigenerate-agent", string version_ = "0.1.0") {
        this.transport = transport;
        this.name = name;
        this.version_ = version_;
        transport.setMessageHandler(&handleMessage);
    }

    /// Convenience constructor using stdio transport.
    this(string name = "nijigenerate-agent", string version_ = "0.1.0") {
        this(createStdioTransport(), name, version_);
    }

    /// Start processing incoming JSON-RPC messages (blocks).
    void start() {
        transport.run();
    }

    /// Send status/progress notification to the editor.
    void sendStatus(StatusNotification notification) {
        JSONValue params = JSONValue([
            "title": JSONValue(notification.title),
            "message": JSONValue(notification.message),
            "level": JSONValue(notification.level.to!int)
        ]);
        transport.sendMessage(JSONValue([
            "jsonrpc": JSONValue(JSONRPC_VERSION),
            "method": JSONValue(ACP_METHOD_STATUS),
            "params": params
        ]));
    }

private:
    void handleMessage(JSONValue message) {
        try {
            auto request = Request.fromJSON(message);

            if (!request.isNotification()) {
                auto response = handleRequest(request);
                transport.sendMessage(response);
            } else {
                handleNotification(request);
            }
        } catch (Exception e) {
            // fall back to generic invalidRequest
            transport.sendMessage(Response.makeError(
                JSONValue(null),
                ErrorCode.invalidRequest,
                e.msg
            ).toJSON());
        }
    }

    JSONValue handleRequest(Request request) {
        // initialize
        if (request.method == ACP_METHOD_INITIALIZE) {
            initialized = true;
            return Response.success(request.id, JSONValue([
                "protocolVersion": JSONValue(ACP_PROTOCOL_VERSION),
                "serverInfo": JSONValue([
                    "name": JSONValue(name),
                    "version": JSONValue(version_)
                ])
            ])).toJSON();
        }

        // ping
        if (request.method == ACP_METHOD_PING) {
            return Response.success(request.id, parseJSON("{}")).toJSON();
        }

        // workspace/applyEdit
        if (request.method == ACP_METHOD_APPLY_EDIT) {
            if (!initialized) {
                return Response.makeError(request.id,
                    ErrorCode.invalidRequest, "Agent not initialized").toJSON();
            }
            if (onApplyEdit is null) {
                return Response.makeError(request.id,
                    ErrorCode.methodNotFound, "applyEdit handler not set").toJSON();
            }

            auto params = request.params;
            if ("uri" !in params || "edits" !in params) {
                return Response.makeError(request.id,
                    ErrorCode.invalidParams, "Missing uri/edits").toJSON();
            }

            WorkspaceEdit edit;
            edit.uri = params["uri"].str;
            foreach (item; params["edits"].array) {
                TextEdit te;
                te.range.start.line = item["range"]["start"]["line"].integer.to!size_t;
                te.range.start.character = item["range"]["start"]["character"].integer.to!size_t;
                te.range.end.line = item["range"]["end"]["line"].integer.to!size_t;
                te.range.end.character = item["range"]["end"]["character"].integer.to!size_t;
                te.newText = item["newText"].str;
                edit.edits ~= te;
            }

            bool applied = onApplyEdit(edit);
            return Response.success(request.id, JSONValue([
                "applied": JSONValue(applied)
            ])).toJSON();
        }

        // editor/userMessage (optional free-form text from editor)
        if (request.method == ACP_METHOD_USER_MSG) {
            if (!initialized) {
                return Response.makeError(request.id,
                    ErrorCode.invalidRequest, "Agent not initialized").toJSON();
            }
            auto params = request.params;
            string text = ("text" in params && params["text"].type == JSONType.string)
                ? params["text"].str : "";
            // For now just acknowledge; concrete agents may override this class and handle text.
            return Response.success(request.id, JSONValue([
                "received": JSONValue(text)
            ])).toJSON();
        }

        return Response.makeError(request.id, ErrorCode.methodNotFound,
            "Unknown method: "~request.method).toJSON();
    }

    void handleNotification(Request notification) {
        // Reserved for future notifications (e.g., diagnostics push from editor)
    }
}
