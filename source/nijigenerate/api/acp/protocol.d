module nijigenerate.api.acp.protocol;

import std.json;

/// ACP protocol version supported by this implementation.
enum int ACP_PROTOCOL_VERSION = 1;

/// JSON-RPC version used by ACP.
enum string JSONRPC_VERSION = "2.0";

/// Standard JSON-RPC error codes.
enum ErrorCode {
    parseError     = -32700,
    invalidRequest = -32600,
    methodNotFound = -32601,
    invalidParams  = -32602,
    internalError  = -32603
}

/// Common ACP method names (handled on the Coding Agent side).
enum string ACP_METHOD_INITIALIZE = "initialize";
enum string ACP_METHOD_PING       = "ping";
enum string ACP_METHOD_APPLY_EDIT = "workspace/applyEdit";
enum string ACP_METHOD_STATUS     = "notifications/status";
enum string ACP_METHOD_USER_MSG   = "editor/userMessage";

/// Base ACP error.
class ACPError : Exception {
    int code;
    string details;

    this(int code, string message, string details = null,
         string file = __FILE__, size_t line = __LINE__) {
        this.code = code;
        this.details = details;
        super(message, file, line);
    }

    JSONValue toJSON() const {
        auto err = JSONValue([
            "code": JSONValue(code),
            "message": JSONValue(msg)
        ]);
        if (details !is null) {
            err["data"] = JSONValue(details);
        }
        return JSONValue([
            "jsonrpc": JSONValue(JSONRPC_VERSION),
            "error": err
        ]);
    }
}
