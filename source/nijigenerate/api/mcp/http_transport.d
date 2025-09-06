/**
 * HTTP transport implementation for MCP.
 *
 * This transport exposes two endpoints:
 *  - POST /mcp   : accepts JSON-RPC messages
 *  - GET  /events: Server-Sent Events stream for responses/notifications
 */
module nijigenerate.api.mcp.http_transport;

import std.json;

import vibe.http.server;
import vibe.http.router;
import vibe.core.core : runApplication, exitEventLoop, yield, runTask;
import core.thread.fiber : Fiber;
import vibe.core.stream : OutputStream;
import vibe.stream.operations : readAllUTF8;
import vibe.internal.interfaceproxy : InterfaceProxy;

import mcp.transport.stdio;
import mcp.server;

class HttpTransport : Transport {
    private {
        void delegate(JSONValue) messageHandler;
        HTTPListener listener;
        InterfaceProxy!OutputStream[] clients;
        JSONValue*[Fiber] responseSlots;
        string host;
        ushort port;
        bool running;
        bool shouldExit; // close() called before or during run()
    }

    // Run on the event loop to stop listening and exit cleanly
    private void performShutdown() nothrow @system {
        try { listener.stopListening(); } catch (Exception) {}
        try { exitEventLoop(); } catch (Exception) {}
    }
    // Stop listening without touching the event loop state (safe after loop exit)
    private void performStopListening() nothrow @system {
        try { listener.stopListening(); } catch (Exception) {}
    }

    this(string host = "127.0.0.1", ushort port = 8080) {
        this.host = host;
        this.port = port;
    }

    void setMessageHandler(void delegate(JSONValue) handler) {
        messageHandler = handler;
    }

    void handleMessage(JSONValue message) {
        if (messageHandler !is null) messageHandler(message);
    }

    void sendMessage(JSONValue message) {
        auto fb = Fiber.getThis();
        synchronized(this) {
            if (fb in responseSlots) {
                *responseSlots[fb] = message;
                responseSlots.remove(fb);
            }
            size_t i = 0;
            while (i < clients.length) {
                auto c = clients[i];
                try {
                    c.write("data: " ~ message.toString() ~ "\n\n");
                    c.flush();
                    ++i;
                } catch (Exception) {
                    clients = clients[0 .. i] ~ clients[i + 1 .. $];
                }
            }
        }
    }

    private void handlePost(scope HTTPServerRequest req, scope HTTPServerResponse res) {
        auto body = req.bodyReader.readAllUTF8();
        JSONValue msg;
        try {
            msg = parseJSON(body);
        } catch (JSONException e) {
            auto err = JSONValue([
                "jsonrpc": JSONValue("2.0"),
                "error": JSONValue([
                    "code": JSONValue(-32700),
                    "message": JSONValue("Parse error")
                ])
            ]);
            res.headers["Content-Type"] = "application/json";
            res.writeBody(err.toString());
            return;
        }

        auto responsePtr = new JSONValue; // heap-allocated to avoid @safe address-of local
        auto fb = Fiber.getThis();
        synchronized(this) responseSlots[fb] = responsePtr;
        handleMessage(msg);
        synchronized(this) responseSlots.remove(fb);

        res.headers["Content-Type"] = "application/json";
        if (!("id" in msg) || msg["id"].type == JSONType.null_) {
            res.statusCode = 204;
            res.writeBody("");
        } else {
            res.writeBody(responsePtr.toString());
        }
    }

    private void handleEvents(scope HTTPServerRequest req, scope HTTPServerResponse res) {
        import core.time : seconds;
        import vibe.core.core : sleep;

        res.headers["Content-Type"] = "text/event-stream";
        res.headers["Cache-Control"] = "no-cache";
        res.headers["Connection"] = "keep-alive";
        auto stream = res.bodyWriter;

        // Immediately send an SSE prelude so clients and proxies flush headers
        // and the client knows the stream is open.
        try {
            stream.write(":\n\n"); // comment per SSE spec
            stream.flush();
        } catch (Exception) {}

        synchronized(this) clients ~= stream;
        scope(exit) synchronized(this) {
            // remove stream from clients if still present
            size_t idx = size_t.max;
            foreach (i, s; clients) {
                if (s is stream) { idx = i; break; }
            }
            if (idx != size_t.max) clients = clients[0 .. idx] ~ clients[idx + 1 .. $];
        }
        // Keep the handler alive and send a heartbeat every ~15s to prevent
        // idle proxies from closing the connection.
        int counter = 0;
        while (running) {
            sleep(5.seconds);
            ++counter;
            if (!running) break;
            if ((counter % 3) == 0) {
                try {
                    stream.write(": heartbeat\n\n");
                    stream.flush();
                } catch (Exception) {
                    break; // client went away
                }
            }
        }
    }

    void run() {
        auto router = new URLRouter;
        router.post("/mcp", &handlePost);
        router.get("/events", &handleEvents);
        auto settings = new HTTPServerSettings;
        settings.port = port;
        settings.bindAddresses = [host];
        listener = listenHTTP(settings, router);
        running = true;
        scope(exit) performStopListening();

        // If close() was called before the event loop starts, exit early.
        if (shouldExit) {
            running = false;
            return;
        }
        string[] unrecognized;
        runApplication(&unrecognized);
    }

    void close() {
        // Mark for early exit and stop accepting new connections ASAP
        shouldExit = true;
        running = false;
        // Proactively close any connected SSE clients to release handles
        synchronized (this) {
            size_t i = 0;
            while (i < clients.length) {
                auto c = clients[i];
                try { c.write("event: shutdown\n\n"); c.flush(); } catch (Exception) {}
                ++i;
            }
            clients.length = 0;
        }
        // Stop listener immediately even if event loop is not running yet
        performStopListening();
        // Stop listener and exit event loop on the event loop thread
        runTask(&performShutdown);
    }
}

HttpTransport createHttpTransport(string host = "127.0.0.1", ushort port = 8080) {
    return new HttpTransport(host, port);
}

class ExtMCPServer : MCPServer {
    Transport _transport;
    /**
     * Constructs an MCPServer with the specified transport.
     *
     * This constructor allows providing a custom transport implementation.
     *
     * Params:
     *   transport = The transport layer to use for communication
     *   name = The server name to report in initialization
     *   version_ = The server version to report in initialization
     */
    this(Transport transport, string name = "D MCP Server", string version_ = "1.0.0") {
        _transport = transport;
        super(transport, name, version_);
    }

    /**
     * Requests the server to stop processing and shut down its transport.
     *
     * This is a thin wrapper that forwards to the underlying transport's
     * close() method to break out of any blocking loops and stop cleanly.
     */
    void stop() {
        if (_transport !is null) {
            _transport.close();
        }
    }
}