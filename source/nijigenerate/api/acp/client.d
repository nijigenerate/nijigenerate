module nijigenerate.api.acp.client;

import std.process : Pid, ProcessPipes, pipeProcess, Config, Redirect;
import std.json : parseJSON, JSONValue, JSONType;
import std.stdio : StdioException;
import std.exception : ErrnoException;
import std.format : format;
import std.string : startsWith, strip, split, indexOf;
import std.algorithm : startsWith;
import std.algorithm.searching : countUntil;
import std.conv : to;
import core.thread;
import core.sync.mutex;
import core.sync.condition;
import core.time : msecs;
version(Posix) import core.sys.posix.unistd : read;
version(Posix) import core.sys.posix.sys.select : select, timeval, fd_set, FD_ZERO, FD_SET;

import mcp.protocol : Request;

import nijigenerate.api.acp.protocol;
import nijigenerate.api.acp.types;
import nijigenerate.api.acp.transport.stdio;
import nijigenerate.core.settings : incSettingsGet;

/// Minimal client for interacting with the Coding Agent from the editor.
/// - Pass commands as string[] (not a shell string) to avoid OS-specific behavior.
/// - You can also inject an existing Transport.
class ACPClient {
    private {
        StdioTransport transport;
        ProcessPipes pipes;
        Pid pid;
        size_t nextId;
        bool ownsProcess;
        bool debugStdout;
        void delegate(string) logger;
        bool initPending;
        JSONValue initResult;
        string initError;
        bool debugEchoPipes = true;
        bool delegate() cancelCheck;
        // inbound buffer handled by background reader thread
        PollResult[] inboundQueue;
        Mutex inboundMutex;
        Condition inboundCond;
        Thread reader;
        bool readerRunning;
        ubyte[] readBuffer;
        Thread stderrThread;
        bool stderrRunning;
        string preferredSendMode = "line"; // "line" or "content-length"
        string sessionId;

    public:
        @property string getInitError() { return initError; }
        @property JSONValue getInitResult() { return initResult; }
        @property Pid getPid() { return pid; }
        @property int getStdoutFd() { return stdoutFd(); }
    }

    PollResult decodeJson(string payload) {
        PollResult pr;
        if (payload.length == 0) return pr;
        try {
            auto json = parseJSON(payload);
            log("recv: "~payload);
            pr.result = json;
            pr.hasValue = true;
        } catch (Exception pe) {
            // Some agents emit JSON as an escaped string (e.g. \"{...}\").
            JSONValue decoded;
            if (tryDecodeEscapedJsonString(payload, decoded)) {
                log("recv: "~decoded.toString());
                pr.result = decoded;
                pr.hasValue = true;
                return pr;
            }
            // Retry once after sanitizing control chars inside JSON strings.
            bool recovered = false;
            try {
                auto sanitized = sanitizeJsonPayload(payload);
                auto json = parseJSON(sanitized);
                log("recv: "~sanitized);
                pr.result = json;
                pr.hasValue = true;
                recovered = true;
            } catch (Exception) {
                // keep original error
            }
            if (!recovered) {
                auto err = drainStderr();
                auto msg = "JSON parse failed: " ~ pe.msg;
                logParseFailure(payload, pe.msg);
                if (err.length) log("stderr: "~err);
                log("state: " ~ stateSnapshot());
                pr.error = msg;
                pr.hasValue = true;
            }
        }
        return pr;
    }

    ptrdiff_t findByteIndex(const(ubyte)[] buf, ubyte value) {
        foreach (i, b; buf) {
            if (b == value) return cast(ptrdiff_t)i;
        }
        return -1;
    }

    ubyte[] truncateAtNull(ubyte[] bytes) {
        foreach (i, b; bytes) {
            if (b == 0) return bytes[0 .. i];
        }
        return bytes;
    }

    bool tryDecodeEscapedJsonString(string payload, out JSONValue result) {
        result = JSONValue.init;
        if (payload.length == 0) return false;
        string candidate = payload;
        if (payload[0] == '\\') {
            if (payload.length < 2) return false;
            if (payload[1] == '{' || payload[1] == '[') {
                // ok
            } else if (payload[1] == '"') {
                if (payload.length < 3 || !(payload[2] == '{' || payload[2] == '[')) {
                    return false;
                }
            } else {
                return false;
            }
            candidate = "\"" ~ payload ~ "\"";
        } else if (payload[0] != '"') {
            return false;
        }
        JSONValue tmp;
        try {
            tmp = parseJSON(candidate);
        } catch (Exception) {
            return false;
        }
        if (tmp.type != JSONType.string) return false;
        try {
            result = parseJSON(tmp.str);
            return true;
        } catch (Exception) {
            return false;
        }
    }

    void logParseFailure(string payload, string errMsg) {
        import std.base64 : Base64;
        import std.conv : to;
        auto rawBytes = cast(const(ubyte)[])payload;
        auto rawB64 = Base64.encode(rawBytes);
        enum size_t kMaxB64 = 600;
        string head = rawB64.length > kMaxB64 ? rawB64[0 .. kMaxB64].idup : rawB64.idup;
        string tail = rawB64.length > kMaxB64 ? rawB64[$ - kMaxB64 .. $].idup : "";
        auto msg = "JSON parse failed: " ~ errMsg ~ " | raw.len=" ~ payload.length.to!string ~
            " | raw.b64.head=" ~ head;
        if (tail.length) msg ~= " | raw.b64.tail=" ~ tail;
        log(msg);
    }

    void logParseFailureBytes(const(ubyte)[] payload, string errMsg) {
        import std.base64 : Base64;
        import std.conv : to;
        auto rawB64 = Base64.encode(payload);
        enum size_t kMaxB64 = 600;
        string head = rawB64.length > kMaxB64 ? rawB64[0 .. kMaxB64].idup : rawB64.idup;
        string tail = rawB64.length > kMaxB64 ? rawB64[$ - kMaxB64 .. $].idup : "";
        auto msg = "JSON parse failed: " ~ errMsg ~ " | raw.len=" ~ payload.length.to!string ~
            " | raw.b64.head=" ~ head;
        if (tail.length) msg ~= " | raw.b64.tail=" ~ tail;
        log(msg);
    }

    string sanitizeJsonPayload(string payload) {
        import std.array : appender;
        auto buf = appender!string();
        bool inString = false;
        bool escaped = false;
        auto bytes = cast(const(ubyte)[])payload;
        for (size_t i = 0; i < bytes.length; ++i) {
            ubyte b = bytes[i];
            if (!inString) {
                if (b == '"') inString = true;
                buf.put(cast(char)b);
                continue;
            }
            if (escaped) {
                escaped = false;
                buf.put(cast(char)b);
                continue;
            }
            if (b == '\\') {
                if (i + 1 < bytes.length) {
                    ubyte next = bytes[i + 1];
                    immutable bool validEscape =
                        next == '"' || next == '\\' || next == '/' ||
                        next == 'b' || next == 'f' || next == 'n' ||
                        next == 'r' || next == 't' || next == 'u';
                    if (validEscape) {
                        escaped = true;
                        buf.put('\\');
                    } else {
                        // Fix invalid escapes like \a by escaping the backslash.
                        buf.put("\\\\");
                        if (next < 0x20) {
                            import std.format : format;
                            buf.put(format("\\u%04x", next));
                        } else {
                            buf.put(cast(char)next);
                        }
                        ++i;
                    }
                } else {
                    buf.put("\\\\");
                }
                continue;
            }
            if (b == '"') {
                inString = false;
                buf.put('"');
                continue;
            }
            if (b < 0x20) {
                // Escape control chars inside strings to keep JSON parser happy.
                switch (b) {
                    case 0x08: buf.put("\\b"); break;
                    case 0x09: buf.put("\\t"); break;
                    case 0x0A: buf.put("\\n"); break;
                    case 0x0C: buf.put("\\f"); break;
                    case 0x0D: buf.put("\\r"); break;
                    default:
                        import std.format : format;
                        buf.put(format("\\u%04x", b));
                        break;
                }
                continue;
            }
            buf.put(cast(char)b);
        }
        return buf.data;
    }

    /// Constructor using an existing Transport (e.g. same process).
    this(StdioTransport transport) {
        this.transport = transport;
        this.ownsProcess = false;
        startReader();
    }

    /// Launch the Coding Agent executable as a child process (OS-agnostic).
    /// `command` should be split, e.g. ["./out/nijigenerate-agent"] or ["node","agent.js"].
    this(string[] command, string cwd = null) {
        // pipeProcess uses CreateProcess/posix spawn internally and avoids shell parsing,
        // which is safer and less OS-dependent.
        // pipeProcess creates stdin/stdout pipes by default
        auto proc = pipeProcess(command, Redirect.all, null, Config.none, cwd);
        pid = proc.pid;
        pipes = proc;
        // Note: input must read from child's stdout, output writes to child's stdin.
        transport = new StdioTransport(pipes.stdout, pipes.stdin);
        ownsProcess = true;
        startReader();
        startStderrReader();
    }

    /// Toggle whether to show stdout on the host side (debug).
    void setDebugStdout(bool enabled) {
        debugStdout = enabled;
    }

    /// Forward raw child stdout/stderr logs to logger (default ON).
    void setDebugEcho(bool enabled) {
        debugEchoPipes = enabled;
    }

    /// Set log output target (e.g. Agent panel).
    void setLogger(void delegate(string) cb) {
        logger = cb;
    }

    /// Set a cancel-check delegate (reads an external flag).
    void setCancelCheck(bool delegate() cb) {
        cancelCheck = cb;
    }

    void log(string msg) {
        if (logger) logger(msg);
    }

    /// Set send mode ("line" or "content-length").
    void setSendMode(string mode) {
        preferredSendMode = (mode == "content-length") ? "content-length" : "line";
    }

    /// Send line-delimited JSON as-is (no Content-Length).
    void sendRawLine(string raw) {
        try {
            pipes.stdin.write(raw ~ "\n");
            pipes.stdin.flush();
            if (debugEchoPipes) log("[stdin-raw] " ~ raw);
        } catch (Exception e) {
            auto err = drainStderr();
            auto m = "sendRaw failed: "~e.msg;
            if (err.length) m ~= " | stderr: "~err;
            m ~= " | state: " ~ stateSnapshot();
            log(m);
            throw new Exception(m);
        }
    }

    string escapeJsonString(string s) {
        import std.array : appender;
        import std.format : format;
        auto w = appender!string();
        foreach(ch; s) {
            switch(ch) {
                case '\\': w.put("\\\\"); break;
                case '"':  w.put("\\\""); break;
                case '\b': w.put("\\b"); break;
                case '\f': w.put("\\f"); break;
                case '\n': w.put("\\n"); break;
                case '\r': w.put("\\r"); break;
                case '\t': w.put("\\t"); break;
                default:
                    if (ch < 0x20) w.put(format("\\u%04x", cast(int)ch));
                    else w.put(ch);
            }
        }
        return w.data;
    }

    /// Send arbitrary user input text via session/prompt.
    string sendPrompt(string text) {
        if (sessionId.length == 0) {
            newSession();
        }
        auto idStr = nextId++.to!string;
        auto textEsc = escapeJsonString(text);
        auto raw = `{"jsonrpc":"2.0","id":` ~ idStr ~ `,"method":"session/prompt","params":{"sessionId":"` ~ sessionId ~ `","prompt":[{"type":"text","text":"` ~ textEsc ~ `"}]}}`;
        sendRawLine(raw);
        return idStr;
    }

    /// session/cancel notification to abort the current prompt turn.
    void cancelPrompt() {
        if (sessionId.length == 0) return;
        auto raw = `{"jsonrpc":"2.0","method":"session/cancel","params":{"sessionId":"` ~ sessionId ~ `"}}`;
        sendRawLine(raw);
    }

    /// respond to session/request_permission
    void sendPermissionResponse(string id, bool granted) {
        if (id.length == 0 || id == "null") {
            throw new Exception("permission response missing request id");
        }
        // Keep backward compatibility (granted: bool) and also include outcome for newer implementations.
        auto outcome = granted ? "granted" : "denied";
        auto raw = `{"jsonrpc":"2.0","id":` ~ id ~ `,"result":{"granted":` ~ (granted ? "true" : "false") ~ `,"outcome":"` ~ outcome ~ `"}}`;
        sendRawLine(raw);
    }

    /// reader thread drains stdout/stderr and buffers results
    void startReader() {
        inboundMutex = new Mutex();
        inboundCond = new Condition(inboundMutex);
        readerRunning = true;
        reader = new Thread(&readerLoop);
        reader.isDaemon(true);
        reader.start();
    }

    void startStderrReader() {
        stderrRunning = true;
        stderrThread = new Thread(&stderrLoop);
        stderrThread.isDaemon(true);
        stderrThread.start();
    }

    PollResult[] takeInbound() {
        inboundMutex.lock();
        auto res = inboundQueue.dup;
        inboundQueue.length = 0;
        inboundMutex.unlock();
        return res;
    }

    /// Send initialize and receive server info.
    JSONValue initialize() {
        auto req = Request(ACP_METHOD_INITIALIZE, buildInitParams(), JSONValue(nextId++));
        send(req.toJSON());
        return waitResponse();
    }

    /// Create a new session (session/new).
    void newSession() {
        auto idStr = nextId++.to!string;
        import std.file : getcwd;
        auto cwd = getcwd();
        auto mcpJson = currentMcpServers();
        auto raw = `{"jsonrpc":"2.0","id":` ~ idStr ~ `,"method":"session/new","params":{"cwd":"`
            ~ escapeJsonString(cwd) ~ `","mcpServers":` ~ mcpJson.toString() ~ `}}`;
        sendRawLine(raw);
        auto res = waitResponse();
        // Expect result.sessionId
        auto result = res["result"];
        if (result.type == JSONType.object && "sessionId" in result.object) {
            sessionId = result["sessionId"].str;
            log("session/new -> " ~ sessionId);
        } else if ("error" in res.object) {
            auto err = res["error"].toString();
            log("session/new error: " ~ err);
            throw new Exception("session/new error: "~err);
        } else {
            throw new Exception("session/new missing sessionId");
        }
    }

    /// Non-blocking initialize: send only, get the result via pollInitialize.
    void initializeAsync() {
        initPending = true;
        initResult = JSONValue.init;
        initError = null;
        auto req = Request(ACP_METHOD_INITIALIZE, buildInitParams(), JSONValue(nextId++));
        send(req.toJSON());
    }

    /// Poll initialize result. Returns: true=done (success or failure), false=not yet.
    bool pollInitialize() {
        if (!initPending) return true;
        auto maybe = popInbound();
        if (!maybe.hasValue) return false;
        if (maybe.error.length) {
            initError = maybe.error;
        } else {
            initResult = maybe.result;
        }
        initPending = false;
        return true;
    }

    JSONValue buildInitParams() {
        JSONValue clientInfo = JSONValue([
            "name": JSONValue("nijigenerate"),
            "version": JSONValue("0.0.0")
        ]);
        return JSONValue([
            "protocolVersion": JSONValue(ACP_PROTOCOL_VERSION),
            "clientInfo": clientInfo
        ]);
    }

    /// Decide which MCP servers to advertise to the agent.
    /// Priority: if embedded MCP HTTP server is enabled, use that;
    /// otherwise fall back to user-provided JSON in ACP.McpServers (array).
    JSONValue currentMcpServers() {
        bool mcpEnabled = incSettingsGet!bool("MCP.Enabled", false);
        auto host = incSettingsGet!string("MCP.Host", "127.0.0.1");
        auto port = incSettingsGet!int("MCP.Port", 8088);
        if (!(mcpEnabled || host.length)) {
            return parseJSON("[]");
        }
        if (!host.length) host = "127.0.0.1";
        auto url = format("http://%s:%s/mcp", host, port);
        // Expected format (union variant http):
        // {"type":"http","name":"nijigenerate","url":"http://host:port","headers":[{"name":"","value":""},...]}
        auto name = incSettingsGet!string("MCP.Name", "nijigenerate");
        string headersRaw = incSettingsGet!string("MCP.Headers", "[]");
        JSONValue headers;
        try {
            headers = parseJSON(headersRaw);
        } catch (Exception) {
            headers = parseJSON("[]");
        }
        if (headers.type != JSONType.array) headers = parseJSON("[]");
        auto jsonStr = `[{` ~
            `"type":"http",` ~
            `"name":"` ~ escapeJsonString(name) ~ `",` ~
            `"url":"` ~ escapeJsonString(url) ~ `",` ~
            `"headers":` ~ headers.toString() ~
        `}]`;
        return parseJSON(jsonStr);
    }

    /// Send ping (success if no exception).
    void ping() {
        auto req = Request(ACP_METHOD_PING, parseJSON("{}"), JSONValue(nextId++));
        send(req.toJSON());
        waitResponse();
    }

    /// Perform shutdown when owning a child process.
    void close() {
        readerRunning = false;
        stderrRunning = false;
        if (ownsProcess && pipes.stdin.isOpen()) {
            pipes.stdin.close();
        }
        if (ownsProcess && pipes.stdout.isOpen()) {
            pipes.stdout.close();
        }
        if (ownsProcess && pipes.stderr.isOpen()) {
            pipes.stderr.close();
        }
        if (inboundCond !is null) inboundCond.notifyAll();
        if (reader !is null) {
            reader.join();
            reader = null;
        }
        if (stderrThread !is null) {
            stderrThread.join();
            stderrThread = null;
        }
    }

private:
    void send(JSONValue msg) {
        auto payload = msg.toString();
        try {
            if (preferredSendMode == "content-length") {
                auto frame = format("Content-Length: %d\r\n\r\n%s", payload.length, payload);
                pipes.stdin.write(frame);
            } else {
                // line-delimited LF (codex-acp expects this)
                pipes.stdin.write(payload ~ "\n");
            }
            pipes.stdin.flush();
            if (debugEchoPipes) log("[stdin] " ~ payload);
        } catch (StdioException e) {
            auto err = drainStderr();
            auto m = "send failed: "~e.msg;
            if (err.length) m ~= " | stderr: "~err;
            m ~= " | state: " ~ stateSnapshot();
            log(m);
            throw new Exception(m);
        } catch (ErrnoException e) {
            auto err = drainStderr();
            auto m = "send failed (errno): "~e.msg;
            if (err.length) m ~= " | stderr: "~err;
            m ~= " | state: " ~ stateSnapshot();
            log(m);
            throw new Exception(m);
        } catch (Exception e) {
            auto err = drainStderr();
            auto m = "send failed: "~e.msg;
            if (err.length) m ~= " | stderr: "~err;
            m ~= " | state: " ~ stateSnapshot();
            log(m);
            throw new Exception(m);
        }
    }

    string stateSnapshot() {
        return format("pid=%s stdin(open=%s) stdout(open=%s,eof=%s) stderr(open=%s,eof=%s)",
            pid, pipes.stdin.isOpen(), pipes.stdout.isOpen(), pipes.stdout.eof, pipes.stderr.isOpen(), pipes.stderr.eof);
    }

    // Made public so AgentPanel worker can inspect stdout without blocking.
    public struct PollResult {
        bool hasValue;
        JSONValue result;
        string error;
    }

    // public getter declared earlier; internal helper here for worker
    int stdoutFd() {
        version(Posix) {
            return pipes.stdout.fileno();
        }
        return -1;
    }

    /// Non-blocking single poll from inbound queue.
    public PollResult tryPollOnce() {
        return popInbound();
    }

    PollResult popInbound() {
        PollResult pr;
        inboundMutex.lock();
        if (inboundQueue.length) {
            pr = inboundQueue[0];
            inboundQueue = inboundQueue[1 .. $];
        }
        inboundMutex.unlock();
        return pr;
    }

    void readerLoop() {
        while (readerRunning) {
            try {
                string chunk;
                bool hasChunk = readStdoutChunk(chunk);
                if (!hasChunk) {
                    if (!readerRunning) break;
                    Thread.sleep(10.msecs);
                    continue;
                }
                if (debugEchoPipes && chunk.length) {
                    import std.base64 : Base64;
                    auto b64 = Base64.encode(cast(const(ubyte)[])chunk);
                    log("[stdout-raw] " ~ cast(string)b64);
                }
                if (chunk.length == 0) continue;
                appendChunkBytes(cast(const(ubyte)[])chunk);
            } catch (Exception e) {
                log("reader exit: "~e.msg);
                break;
            }
        }
    }

    void appendChunkBytes(const(ubyte)[] chunk) {
        readBuffer ~= chunk;
        parseReadBuffer();
    }

    void parseReadBuffer() {
        while (readBuffer.length) {
            auto nl = findByteIndex(readBuffer, cast(ubyte)'\n');
            if (nl < 0) break;
            auto lineBytes = readBuffer[0 .. cast(size_t)nl];
            if (lineBytes.length && lineBytes[$ - 1] == '\r') {
                lineBytes = lineBytes[0 .. $ - 1];
            }
            readBuffer = readBuffer[cast(size_t)nl + 1 .. $];
            lineBytes = truncateAtNull(lineBytes);
            if (lineBytes.length == 0) continue;
            auto pr = decodeJson(cast(string)lineBytes);
            if (pr.error.length) {
                logParseFailureBytes(lineBytes, pr.error);
            }
            enqueueResult(pr);
        }
    }


    void enqueueResult(PollResult pr) {
        if (!pr.hasValue) return;
        inboundMutex.lock();
        inboundQueue ~= pr;
        inboundCond.notify();
        inboundMutex.unlock();
    }

    /// read available stdout into buffer with timeout (ms). returns true if any bytes read.

    JSONValue waitResponse() {
        try {
            while (true) {
                if (cancelCheck !is null && cancelCheck()) {
                    throw new Exception("cancelled");
                }
                auto pr = popInbound();
                if (pr.hasValue) {
                    if (pr.error.length) throw new Exception(pr.error);
                    return pr.result;
                }
                inboundMutex.lock();
                inboundCond.wait(50.msecs);
                inboundMutex.unlock();
            }
        } catch (StdioException e) {
            auto err = drainStderr();
            auto msg = "Agent stream closed: "~e.msg;
            if (err.length) msg ~= " | stderr: "~err;
            msg ~= " | state: " ~ stateSnapshot();
            log(msg);
            throw new Exception(msg);
        } catch (Exception e) {
            auto err = drainStderr();
            auto msg = e.msg;
            if (err.length) msg ~= " | stderr: "~err;
            msg ~= " | state: " ~ stateSnapshot();
            log(msg);
            throw new Exception(msg);
        }
    }

    string drainStderr() {
        string output;
        for (;;) {
            string chunk;
            if (!readStderrChunk(chunk)) break;
            if (chunk.length == 0) break;
            output ~= chunk;
            if (debugEchoPipes) log("[stderr] " ~ chunk);
        }
        return output;
    }

    void stderrLoop() {
        while (stderrRunning) {
            try {
                string chunk;
                bool hasChunk = readStderrChunk(chunk);
                if (!hasChunk) {
                    if (!stderrRunning) break;
                    Thread.sleep(10.msecs);
                    continue;
                }
                if (chunk.length == 0) continue;
                log("[stderr] " ~ chunk);
            } catch (Exception) {
                break;
            }
        }
    }

    bool readStdoutChunk(out string chunk) {
        chunk = null;
        if (!pipes.stdout.isOpen()) return false;
        version(Posix) {
            auto fd = pipes.stdout.fileno();
            if (fd < 0) return false;
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            int ready = select(fd + 1, &rfds, null, null, &tv);
            if (ready <= 0) return false;
            ubyte[4096] buf;
            auto n = read(fd, buf.ptr, buf.length);
            if (n <= 0) return false;
            auto data = buf[0 .. n].dup;
            chunk = cast(string)data;
            return true;
        } else version(Windows) {
            import core.sys.windows.windows : HANDLE, DWORD, PeekNamedPipe, ReadFile, INVALID_HANDLE_VALUE;
            HANDLE h = pipes.stdout.windowsHandle;
            if (h is null || h == INVALID_HANDLE_VALUE) return false;
            DWORD bytesAvail = 0;
            auto ok = PeekNamedPipe(h, null, 0, null, &bytesAvail, null);
            if (ok == 0 || bytesAvail == 0) return false;
            ubyte[4096] buf;
            DWORD toRead = bytesAvail < buf.length ? bytesAvail : cast(DWORD)buf.length;
            DWORD bytesRead = 0;
            ok = ReadFile(h, buf.ptr, toRead, &bytesRead, null);
            if (ok == 0 || bytesRead == 0) return false;
            auto data = buf[0 .. bytesRead].dup;
            chunk = cast(string)data;
            return true;
        } else {
            return false;
        }
    }

    bool readStderrChunk(out string chunk) {
        chunk = null;
        if (!pipes.stderr.isOpen()) return false;
        version(Posix) {
            auto fd = pipes.stderr.fileno();
            if (fd < 0) return false;
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            int ready = select(fd + 1, &rfds, null, null, &tv);
            if (ready <= 0) return false;
            ubyte[4096] buf;
            auto n = read(fd, buf.ptr, buf.length);
            if (n <= 0) return false;
            auto data = buf[0 .. n].dup;
            chunk = cast(string)data;
            return true;
        } else version(Windows) {
            import core.sys.windows.windows : HANDLE, DWORD, PeekNamedPipe, ReadFile, INVALID_HANDLE_VALUE;
            HANDLE h = pipes.stderr.windowsHandle;
            if (h is null || h == INVALID_HANDLE_VALUE) return false;
            DWORD bytesAvail = 0;
            auto ok = PeekNamedPipe(h, null, 0, null, &bytesAvail, null);
            if (ok == 0 || bytesAvail == 0) return false;
            ubyte[4096] buf;
            DWORD toRead = bytesAvail < buf.length ? bytesAvail : cast(DWORD)buf.length;
            DWORD bytesRead = 0;
            ok = ReadFile(h, buf.ptr, toRead, &bytesRead, null);
            if (ok == 0 || bytesRead == 0) return false;
            auto data = buf[0 .. bytesRead].dup;
            chunk = cast(string)data;
            return true;
        } else {
            return false;
        }
    }
}
