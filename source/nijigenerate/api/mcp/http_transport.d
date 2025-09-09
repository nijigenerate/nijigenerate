/** 
 * HTTP transport implementation for MCP with minimal OAuth2.1-style authorization
 * + Native in-process approval bus (shared queue between threads).
 *
 * Endpoints:
 * - POST /mcp          : JSON-RPC
 * - GET  /events       : SSE from MCP server (protected)
 *
 * OAuth/Discovery:
 * - GET  /.well-known/oauth-protected-resource/mcp
 * - GET  /auth/.well-known/oauth-authorization-server
 * - GET  /auth/authorize   (no HTML; enqueues to in-process bus; waits for decision; then 302 redirect)
 * - POST /auth/token
 *
 * Notes:
 * - Local-only demo: opaque tokens in memory, PKCE(S256) required, RFC8707 `resource` required.
 * - Bind to 127.0.0.1 by default. For production, add HTTPS and hardened storage/jwt.
 */
module nijigenerate.api.mcp.http_transport;

import std.json;
import std.conv;
import std.string : startsWith, format;
import std.algorithm;
import std.array;
import std.exception;
import std.random;
import std.typecons : Nullable, nullable, tuple;
import core.thread.fiber : Fiber;
import core.time;

import vibe.http.server;
import vibe.http.router;
import vibe.http.client;
import vibe.core.core : runApplication, exitEventLoop, yield, runTask, setTimer, sleep;
import vibe.core.stream : OutputStream, InputStream;
import vibe.stream.operations : readAllUTF8;
import vibe.stream.tls;
import std.uri : decodeComponent;

import mcp.transport.stdio;
import mcp.server;
import nijigenerate.api.mcp.task;
import nijigenerate.api.mcp.auth;
import nijigenerate.api.mcp.https : ngCreateSelfSignedCertificate;
import nijigenerate.core.settings; // incSettingsGet

// ======================= HTTP Transport =======================
class HttpTransport : Transport {
    private {
        void delegate(JSONValue) messageHandler;
        HTTPListener listener;
        alias ClientStream = typeof((cast(HTTPServerResponse) null).bodyWriter);
        ClientStream[] clients; // SSE clients for /events
        JSONValue*[Fiber] responseSlots;
        string host;
        ushort port;
        bool running;
        bool shouldExit;
        bool loopExited;
        // Run on the event loop to stop listening and exit cleanly
        private void performShutdown() nothrow @system {
            try { listener.stopListening(); } catch (Exception) {}
            try { exitEventLoop(); } catch (Exception) {}
        }
        // Stop listening without touching the event loop state (safe after loop exit)
        private void performStopListening() nothrow @system {
            try { listener.stopListening(); } catch (Exception) {}
        }

        // ===== Authorization (minimal) =====
        struct AuthCode {
            string clientId;
            string redirectUri;
            string codeChallenge; // PKCE S256
            string scopeId;         // "mcp:use"
            string resource;      // bound to this MCP
            SysTime expiresAt;
        }
        struct AccessToken {
            string clientId;
            string subject;   // "local-user"
            string scopeId;     // "mcp:use"
            string resource;  // canonical resource
            SysTime expiresAt;
        }
        AccessToken[string] atDb; // access_token -> AccessToken
        AuthCode[string]    codeDb; // code -> AuthCode
        string[string]      rtDb; // refresh_token -> access_token

        // Discovery values
        bool   _authEnabled = true;
        string issuer;              // e.g. http://127.0.0.1:8080/auth
        string canonicalResource;   // e.g. http://127.0.0.1:8080/mcp
    }

    this(string host = "127.0.0.1", ushort port = 8080) {
        this.host = host;
        this.port = port;
        issuer            = selfBase() ~ "/auth";
        canonicalResource = selfBase() ~ "/mcp";
    }

    bool authEnabled() { return _authEnabled; }
    void authEnabled(bool value) { _authEnabled = value; }

    void setMessageHandler(void delegate(JSONValue) handler) { 
        messageHandler = handler; 
    }
    // ===== Basic MCP message handling =====
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
private:
    void setUnauthorized(scope HTTPServerResponse res) {
        res.statusCode = 401;
        res.headers["WWW-Authenticate"] =
            `Bearer resource_metadata="` ~ protectedResourceMetadataUrl() ~ `"`;
        res.headers["Content-Type"] = "application/json";
        JSONValue body = ["error": "invalid_token"];
        res.writeBody(body.toString());
    }
    bool checkToken(string token, string resource) {
        import std.datetime : Clock;
        if (token in atDb) {
            auto t = atDb[token];
            if (Clock.currTime() < t.expiresAt && resource.startsWith(t.resource)) return true;
        }
        return false;
    }
    string selfBase() { return "http://" ~ host ~ ":" ~ to!string(port); }
    string protectedResourceMetadataUrl() { return selfBase() ~ "/.well-known/oauth-protected-resource/mcp"; }

    // ===== Utils for tokens/PKCE =====
    import std.base64 : Base64URLNoPadding;
    import std.digest.sha;
    import std.datetime : Clock, SysTime;

    string b64url(in ubyte[] data) {
        return cast(string) Base64URLNoPadding.encode(data);
    }
    string sha256_b64url(string s) {
        ubyte[32] result;
        auto ctx = SHA256();
        ctx.put(cast(ubyte[])s);
        result[] = ctx.finish();
        return b64url(result[]);
    }
    string randToken(string prefix) {
        ubyte[32] rnd;
        foreach (i; 0..rnd.length) rnd[i] = cast(ubyte)uniform(0, 256); // local-only min impl
        return prefix ~ b64url(rnd[]);
    }

    // ===== HTTP Handlers =====
    private void handlePost(scope HTTPServerRequest req, scope HTTPServerResponse res) {
        if (authEnabled) {
            auto auth = req.headers.get("Authorization", "");
            if (!auth.startsWith("Bearer ")) { setUnauthorized(res); return; }
            auto tok = auth["Bearer ".length .. $];
            if (!checkToken(tok, canonicalResource)) { setUnauthorized(res); return; }
        }

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
        if (authEnabled) {
            auto auth = req.headers.get("Authorization", "");
            import std.stdio;
            writefln("[MCP/HTTP] check token=%s",auth);
            if (!auth.startsWith("Bearer ")) { setUnauthorized(res); return; }
            auto tok = auth["Bearer ".length .. $];
            if (!checkToken(tok, canonicalResource)) { setUnauthorized(res); return; }
        }
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

        while (running) {
            sleep(10.seconds);
            if (!running) break;
            try { 
                stream.write(": heartbeat\n\n"); 
                stream.flush(); 
            } catch (Exception) {
                break; // client went away
            }
        }
    }

    // ---- OAuth well-known (Protected Resource Metadata) ----
    void handleProtectedResourceMetadata(scope HTTPServerRequest req, scope HTTPServerResponse res) {
        import std.stdio;
        writefln("[MCP/HTTP] Protected Resource Metadata");
        res.headers["Content-Type"] = "application/json";
        JSONValue payload = [
            "resource": JSONValue(canonicalResource),
            "authorization_servers": JSONValue([issuer]),
            "bearer_methods_supported": JSONValue(["header"]),
            "scopes_supported": JSONValue(["mcp:use"]),
            "resource_name": JSONValue("Local MCP Server")
        ];
        res.writeBody(payload.toString());
    }

    // ---- OAuth AS metadata (RFC 8414) ----
    void handleASMetadata(scope HTTPServerRequest req, scope HTTPServerResponse res) {
        import std.stdio;
        writefln("[MCP/HTTP] AS Metadata");
        res.headers["Content-Type"] = "application/json";
        JSONValue payload = [
            "issuer": JSONValue(issuer),
            "authorization_endpoint": JSONValue(issuer ~ "/authorize"),
            "token_endpoint": JSONValue(issuer ~ "/token"),
            "response_types_supported": JSONValue(["code"]),
            "grant_types_supported": JSONValue(["authorization_code","refresh_token"]),
            "code_challenge_methods_supported": JSONValue(["S256"]),
            "token_endpoint_auth_methods_supported": JSONValue(["none"]),
            "scopes_supported": JSONValue(["mcp:use"])
        ];
        res.writeBody(payload.toString());
    }

    // ---- Authorize（Waiting decision → 302） ----
    void handleAuthorize(scope HTTPServerRequest req, scope HTTPServerResponse res) {
        import std.stdio;
        writefln("[MCP/HTTP] Authorization.");
        auto q = req.query;
        auto clientId     = q.get("client_id", "mcp-local");
        auto redirectUri  = q.get("redirect_uri", "");
        auto responseType = q.get("response_type", "");
        auto scopeId      = q.get("scope", "mcp:use");
        auto state        = q.get("state", "");
        auto codeChal     = q.get("code_challenge", "");
        auto chalMethod   = q.get("code_challenge_method", "S256");
        auto resource     = q.get("resource", "");

        writefln("[MCP/HTTP] responseType=%s, redirectUri=%s resource=%s, canonicalResource=%s, chalMethod=%s", responseType, redirectUri, resource, canonicalResource, chalMethod);
        // validate (PKCE S256 & resource REQUIRED; loopback/http(s) redirect)
        if (responseType != "code" || redirectUri.length == 0 || !canonicalResource.startsWith(resource) || chalMethod != "S256") {
            res.statusCode = 400; res.writeBody("invalid_request"); return;
        }
        if (!redirectUri.startsWith("http://127.0.0.1") &&
            !redirectUri.startsWith("http://localhost") &&
            !redirectUri.startsWith("https://")) {
            res.statusCode = 400; res.writeBody("invalid_redirect_uri"); return;
        }

        // create and enqueue approval request.
        string reqId = randToken("req_");
        string decision = ngSimpleAuth(ApprovalRequest(
            reqId, clientId, scopeId, resource, state, redirectUri
        ));

        if (decision.length == 0) decision = "deny"; // timeout

        if (decision == "approve") {
            // Issue code (1 year, disappeared when application is shutdown.)
            auto code = randToken("code_");
            codeDb[code] = AuthCode(clientId, redirectUri, codeChal, scopeId, resource,
                                    Clock.currTime() + 365.days);
            auto loc = redirectUri ~ "?code=" ~ code ~ "&state=" ~ state;
            res.statusCode = 302; res.headers["Location"] = loc; res.writeBody("");
        } else {
            auto loc = redirectUri ~ "?error=access_denied&state=" ~ state;
            res.statusCode = 302; res.headers["Location"] = loc; res.writeBody("");
        }
    }

    // ---- Token endpoint ----
    void handleToken(scope HTTPServerRequest req, scope HTTPServerResponse res) {
        import std.stdio;
        writefln("[MCP/HTTP] Token.");
        auto body = req.bodyReader.readAllUTF8();
        string[string] form;
        foreach (p; body.split("&")) {
            auto kv = p.splitter("=");
            auto k = kv.front; kv.popFront();
            auto v = kv.empty ? "" : kv.front;
            form[decodeComponent(k)] = decodeComponent(v);
        }
        auto grantType = form.get("grant_type", "");
        res.headers["Content-Type"] = "application/json";

        if (grantType == "authorization_code") {
            auto code         = form.get("code", "");
            auto redirectUri  = form.get("redirect_uri", "");
            auto clientId     = form.get("client_id", "mcp-local");
            auto codeVerifier = form.get("code_verifier", "");
            auto resource     = form.get("resource", "");

            if (!(code in codeDb)) { res.statusCode=400; res.writeBody(`{"error":"invalid_grant"}`); return; }
            auto ac = codeDb[code]; codeDb.remove(code);

            if (Clock.currTime() > ac.expiresAt || ac.redirectUri != redirectUri || ac.clientId != clientId) {
                res.statusCode=400; res.writeBody(`{"error":"invalid_grant"}`); return;
            }
            if (sha256_b64url(codeVerifier) != ac.codeChallenge) {
                res.statusCode=400; res.writeBody(`{"error":"invalid_grant"}`); return;
            }
            if (resource != ac.resource) { res.statusCode=400; res.writeBody(`{"error":"invalid_target"}`); return; }

            auto at = randToken("at_");
            auto rt = randToken("rt_");
            atDb[at] = AccessToken(ac.clientId, "local-user", ac.scopeId, ac.resource, Clock.currTime() + 3600.seconds);
            rtDb[rt] = at;

            JSONValue result = [
                "access_token": JSONValue(at),
                "token_type": JSONValue("Bearer"),
                "expires_in": JSONValue(3600),
                "refresh_token": JSONValue(rt),
                "scope": JSONValue(ac.scopeId)
            ];
            res.statusCode = 200; res.writeBody(result.toString()); return;
        }
        else if (grantType == "refresh_token") {
            auto rt       = form.get("refresh_token", "");
            auto resource = form.get("resource", "");
            if (!(rt in rtDb)) { res.statusCode=400; res.writeBody(`{"error":"invalid_grant"}`); return; }
            auto oldAt = rtDb[rt];
            if (!(oldAt in atDb)) { res.statusCode=400; res.writeBody(`{"error":"invalid_grant"}`); return; }
            auto old = atDb[oldAt];
            if (old.resource != resource) { res.statusCode=400; res.writeBody(`{"error":"invalid_target"}`); return; }

            auto at = randToken("at_");
            atDb[at] = AccessToken(old.clientId, old.subject, old.scopeId, old.resource, Clock.currTime() + 3600.seconds);
            rtDb[rt] = at;

            JSONValue result = [
                "access_token": JSONValue(at),
                "token_type": JSONValue("Bearer"),
                "expires_in": JSONValue(3600),
                "refresh_token": JSONValue(rt),
                "scope": JSONValue(old.scopeId)
            ];
            res.statusCode=200; res.writeBody(result.toString()); return;
        }

        res.statusCode = 400; res.writeBody(`{"error":"unsupported_grant_type"}`);
    }

public:

    void run() {
        import std.stdio : writefln;
        writefln("[MCP/HTTP] run(): binding on %s:%s", host, port);
        auto router = new URLRouter;

        // MCP protected endpoints
        router.post("/mcp",    &handlePost);
        router.get ("/events", &handleEvents);

        // OAuth/Discovery
        router.get("/.well-known/oauth-protected-resource/mcp", &handleProtectedResourceMetadata);
        router.get("/auth/.well-known/oauth-authorization-server", &handleASMetadata);
        router.get("/auth/authorize", &handleAuthorize);
        router.post("/auth/token",    &handleToken);

        // settings
        auto settings = new HTTPServerSettings;

        // HTTPS (self-signed) optional
        bool enabledSSL = incSettingsGet!bool("MCP.https", true);
        if (enabledSSL) {
            import std.path : buildPath;
            import std.file : mkdirRecurse, exists;
            import std.string : toStringz;
            import nijigenerate.core.path : incGetAppConfigPath;
            auto outDir = buildPath(incGetAppConfigPath(), "mcp");
            if (!exists(outDir)) mkdirRecurse(outDir);
            auto certPath = buildPath(outDir, "server.crt");
            auto keyPath  = buildPath(outDir, "server.key");
            ngCreateSelfSignedCertificate(certPath.toStringz, keyPath.toStringz);
            settings.tlsContext = createTLSContext(TLSContextKind.server);
            settings.tlsContext.useCertificateChainFile(certPath);
            settings.tlsContext.usePrivateKeyFile(keyPath);
        }

        // Start server
        settings.port = port;
        settings.bindAddresses = [host];
        listener = listenHTTP(settings, router);

        running = true;
        scope(exit) performStopListening();

        if (shouldExit) { 
            running = false; 
            return; 
        }

        setTimer(100.msecs, { 
            if (shouldExit) { 
                try exitEventLoop(); catch (Exception) {}
            }
        }, true);

        writefln("[MCP/HTTP] run(): listening on %s:%s", host, port);
        runApplication();
        loopExited = true;
        writefln("[MCP/HTTP] run(): event loop exited");
    }

    void close() {
        import std.stdio : writefln;
        writefln("[MCP/HTTP] close(): request transport shutdown");
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

    bool hasExited() const @nogc nothrow @safe { 
        return loopExited; 
    }
}

// Factory
HttpTransport createHttpTransport(string host = "127.0.0.1", ushort port = 8080) {
    return new HttpTransport(host, port);
}

// Optional extended MCP server wrapper
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

    bool transportExited() const {
        auto ht = cast(HttpTransport) _transport;
        return ht is null || ht.hasExited();
    }
}
