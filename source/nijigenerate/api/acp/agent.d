module nijigenerate.api.acp.agent;

import std.json;
import std.array : join;
import std.algorithm : startsWith;
import std.conv : to;
import std.json : parseJSON;
import std.random : uniform;
import std.string : strip;
import core.sync.mutex : Mutex;

import mcp.protocol : Request, Response;
import mcp.protocol : ErrorCode;

import nijigenerate.api.acp.protocol;
import nijigenerate.api.acp.types;
import nijigenerate.api.acp.transport.stdio;

alias ApplyEditHandler = bool delegate(WorkspaceEdit);
alias PromptHandler = string delegate(string sessionId, JSONValue[] promptBlocks, bool delegate() isCancelled);

/// Minimal CodingAgent-side ACP server.
class ACPAgent {
    private {
        struct SessionState {
            bool promptActive;
            bool cancelRequested;
            JSONValue pendingPromptId;
        }

        Transport transport;
        string name;
        string version_;
        bool initialized;
        string[string] sessionOrder;
        SessionState[string] sessions;
        Mutex stateMutex;
    }

    /// Optional handler invoked when editor requests workspace/applyEdit.
    ApplyEditHandler onApplyEdit;
    /// Optional handler invoked when the client sends session/prompt.
    PromptHandler onPrompt;

    this(Transport transport, string name = "nijigenerate-agent", string version_ = "0.1.0") {
        this.transport = transport;
        this.name = name;
        this.version_ = version_;
        this.stateMutex = new Mutex;
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

    void sendSessionUpdate(string sessionId, JSONValue update) {
        transport.sendMessage(JSONValue([
            "jsonrpc": JSONValue(JSONRPC_VERSION),
            "method": JSONValue(ACP_METHOD_SESSION_UPDATE),
            "params": JSONValue([
                "sessionId": JSONValue(sessionId),
                "update": update
            ])
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
                "agentCapabilities": JSONValue([
                    "loadSession": JSONValue(false),
                    "promptCapabilities": JSONValue([
                        "image": JSONValue(false),
                        "audio": JSONValue(false),
                        "embeddedContext": JSONValue(false)
                    ]),
                    "mcpCapabilities": JSONValue([
                        "http": JSONValue(false),
                        "sse": JSONValue(false)
                    ]),
                    "sessionCapabilities": JSONValue(JSONType.object)
                ]),
                "agentInfo": JSONValue([
                    "name": JSONValue(name),
                    "title": JSONValue(name),
                    "version": JSONValue(version_)
                ]),
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

        if (request.method == ACP_METHOD_SESSION_NEW) {
            if (!initialized) {
                return Response.makeError(request.id,
                    ErrorCode.invalidRequest, "Agent not initialized").toJSON();
            }
            auto sessionId = newSessionId();
            stateMutex.lock();
            sessions[sessionId] = SessionState(false, false, JSONValue(null));
            sessionOrder[sessionId] = sessionId;
            stateMutex.unlock();
            return Response.success(request.id, JSONValue([
                "sessionId": JSONValue(sessionId)
            ])).toJSON();
        }

        if (request.method == ACP_METHOD_SESSION_PROMPT) {
            if (!initialized) {
                return Response.makeError(request.id,
                    ErrorCode.invalidRequest, "Agent not initialized").toJSON();
            }

            auto params = request.params;
            if ("sessionId" !in params || params["sessionId"].type != JSONType.string) {
                return Response.makeError(request.id,
                    ErrorCode.invalidParams, "Missing sessionId").toJSON();
            }
            if ("prompt" !in params || params["prompt"].type != JSONType.array) {
                return Response.makeError(request.id,
                    ErrorCode.invalidParams, "Missing prompt").toJSON();
            }

            auto sessionId = params["sessionId"].str;
            stateMutex.lock();
            auto found = sessionId in sessions;
            if (!found) {
                stateMutex.unlock();
                return Response.makeError(request.id,
                    ErrorCode.invalidParams, "Unknown sessionId").toJSON();
            }
            if (sessions[sessionId].promptActive) {
                stateMutex.unlock();
                return Response.makeError(request.id,
                    ErrorCode.invalidRequest, "Prompt already active for session").toJSON();
            }
            sessions[sessionId].promptActive = true;
            sessions[sessionId].cancelRequested = false;
            sessions[sessionId].pendingPromptId = request.id;
            stateMutex.unlock();

            auto promptBlocks = params["prompt"].array.dup;
            auto text = renderPromptText(promptBlocks);
            sendSessionUpdate(sessionId, JSONValue([
                "sessionUpdate": JSONValue("agent_message_chunk"),
                "content": JSONValue([
                    "type": JSONValue("text"),
                    "text": JSONValue(text)
                ])
            ]));

            bool cancelled;
            auto replyText = buildPromptReply(sessionId, promptBlocks, cancelled);
            if (replyText.length && !cancelled) {
                sendSessionUpdate(sessionId, JSONValue([
                    "sessionUpdate": JSONValue("agent_message_chunk"),
                    "content": JSONValue([
                        "type": JSONValue("text"),
                        "text": JSONValue(replyText)
                    ])
                ]));
            }

            stateMutex.lock();
            sessions[sessionId].promptActive = false;
            sessions[sessionId].pendingPromptId = JSONValue(null);
            cancelled = cancelled || sessions[sessionId].cancelRequested;
            sessions[sessionId].cancelRequested = false;
            stateMutex.unlock();

            return Response.success(request.id, JSONValue([
                "stopReason": JSONValue(cancelled ? "cancelled" : "end_turn")
            ])).toJSON();
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
        if (notification.method == ACP_METHOD_SESSION_CANCEL) {
            auto params = notification.params;
            if ("sessionId" in params && params["sessionId"].type == JSONType.string) {
                auto sessionId = params["sessionId"].str;
                stateMutex.lock();
                if (sessionId in sessions) {
                    sessions[sessionId].cancelRequested = true;
                }
                stateMutex.unlock();
            }
        }
    }

    string newSessionId() {
        return "sess_" ~ uniform(0UL, ulong.max).to!string;
    }

    string renderPromptText(JSONValue[] promptBlocks) {
        string[] parts;
        foreach (block; promptBlocks) {
            if (block.type != JSONType.object || "type" !in block.object) continue;
            auto kind = block["type"].str;
            if (kind == "text" && "text" in block.object && block["text"].type == JSONType.string) {
                parts ~= block["text"].str;
                continue;
            }
            if ((kind == "resource_link" || kind == "resourceLink") && "uri" in block.object && block["uri"].type == JSONType.string) {
                parts ~= "[resource] " ~ block["uri"].str;
                continue;
            }
            if (kind == "resource" && "resource" in block.object && block["resource"].type == JSONType.object) {
                auto resource = block["resource"];
                if ("uri" in resource.object && resource["uri"].type == JSONType.string) {
                    parts ~= "[resource] " ~ resource["uri"].str;
                    continue;
                }
            }
        }
        auto text = parts.join("\n").strip;
        if (text.length == 0) return "Received prompt.";
        return "Received prompt:\n" ~ text;
    }

    string buildPromptReply(string sessionId, JSONValue[] promptBlocks, out bool cancelled) {
        cancelled = false;
        auto isCancelled = {
            stateMutex.lock();
            scope(exit) stateMutex.unlock();
            return (sessionId in sessions) && sessions[sessionId].cancelRequested;
        };

        if (isCancelled()) {
            cancelled = true;
            return "";
        }

        if (onPrompt !is null) {
            auto result = onPrompt(sessionId, promptBlocks, isCancelled);
            if (isCancelled()) {
                cancelled = true;
                return "";
            }
            return result;
        }

        return "Prompt processed.";
    }
}
