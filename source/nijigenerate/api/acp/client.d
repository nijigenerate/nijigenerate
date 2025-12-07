module nijigenerate.api.acp.client;

import std.process : Pid, ProcessPipes, pipeProcess, Config, Redirect;
import std.json : parseJSON, JSONValue, JSONType;
import std.stdio : StdioException;
import std.exception : ErrnoException;
import std.format : format;
import std.string : startsWith, strip, split;
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

/// Editor側からCoding Agentと対話する最小クライアント。
/// - OS依存を避けるため、コマンドはシェル文字列ではなく string[] で渡す。
/// - 既存の Transport を注入することも可能。
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
        string readBuffer;
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
        try {
            auto json = parseJSON(payload);
            log("recv: "~payload);
            pr.result = json;
            pr.hasValue = true;
        } catch (Exception pe) {
            auto err = drainStderr();
            auto msg = "JSON parse failed: "~pe.msg~" | line="~payload;
            if (err.length) msg ~= " | stderr: "~err;
            msg ~= " | state: " ~ stateSnapshot();
            log(msg);
            pr.error = msg;
            pr.hasValue = true;
        }
        return pr;
    }

    /// 既存Transportを使うコンストラクタ（同一プロセス内など）。
    this(StdioTransport transport) {
        this.transport = transport;
        this.ownsProcess = false;
        startReader();
    }

    /// Coding Agent 実行ファイルを子プロセスとして起動（OS非依存）。
    /// `command` は ["./out/nijigenerate-agent"] や ["node","agent.js"] のように分割済みで渡す。
    this(string[] command, string cwd = null) {
        // pipeProcess は内部でプラットフォーム毎の適切なCreateProcess/posix spawnを使用し、
        // シェル解釈を行わないため安全かつOS依存が少ない。
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

    /// stdoutをそのままホスト側に表示するかどうかを切り替える（デバッグ用）。
    void setDebugStdout(bool enabled) {
        debugStdout = enabled;
    }

    /// 子プロセス stdout/stderr の生ログを logger に流す（デフォルトON）
    void setDebugEcho(bool enabled) {
        debugEchoPipes = enabled;
    }

    /// ログ出力先（Agentパネルなど）を設定
    void setLogger(void delegate(string) cb) {
        logger = cb;
    }

    /// キャンセルチェック（外部フラグを参照するデリゲート）を設定
    void setCancelCheck(bool delegate() cb) {
        cancelCheck = cb;
    }

    void log(string msg) {
        if (logger) logger(msg);
    }

    /// 送信モードを指定 ("line" または "content-length")
    void setSendMode(string mode) {
        preferredSendMode = (mode == "content-length") ? "content-length" : "line";
    }

    /// 行JSONをそのまま送る（Content-Lengthなし）
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

    /// 任意のユーザ入力テキストを session/prompt で送る。
    void sendPrompt(string text) {
        if (sessionId.length == 0) {
            newSession();
        }
        auto idStr = nextId++.to!string;
        auto textEsc = escapeJsonString(text);
        auto raw = `{"jsonrpc":"2.0","id":` ~ idStr ~ `,"method":"session/prompt","params":{"sessionId":"` ~ sessionId ~ `","prompt":[{"type":"text","text":"` ~ textEsc ~ `"}]}}`;
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

    /// initializeを送り、サーバ情報を受け取る。
    JSONValue initialize() {
        auto req = Request(ACP_METHOD_INITIALIZE, buildInitParams(), JSONValue(nextId++));
        send(req.toJSON());
        return waitResponse();
    }

    /// 新規セッション作成（session/new）
    void newSession() {
        auto idStr = nextId++.to!string;
        import std.file : getcwd;
        auto cwd = getcwd();
        auto raw = `{"jsonrpc":"2.0","id":` ~ idStr ~ `,"method":"session/new","params":{"cwd":"` ~ escapeJsonString(cwd) ~ `","mcpServers":[]}}`;
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

    /// ノンブロッキング版 initialize: 送信だけ行い、結果は pollInitialize で取得
    void initializeAsync() {
        initPending = true;
        initResult = JSONValue.init;
        initError = null;
        auto req = Request(ACP_METHOD_INITIALIZE, buildInitParams(), JSONValue(nextId++));
        send(req.toJSON());
    }

    /// initialize の結果をポーリング。返値: true=完了/失敗いずれも終了, false=まだ
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

    /// pingを送る（例外が出なければ成功）。
    void ping() {
        auto req = Request(ACP_METHOD_PING, parseJSON("{}"), JSONValue(nextId++));
        send(req.toJSON());
        waitResponse();
    }

    /// 子プロセスを持っている場合に終了処理を行う。
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
                auto line = pipes.stdout.readln();
                if (line.length == 0) continue;
                auto raw = line;
                if (debugEchoPipes) {
                    import std.base64 : Base64;
                    auto b64 = Base64.encode(cast(const(ubyte)[])raw);
                    log("[stdout-raw] " ~ cast(string)b64);
                }
                line = line.strip();
                if (line.length == 0) continue;
                PollResult pr;
                if (line.startsWith("Content-Length")) {
                    auto parts = line.split(":");
                    size_t len = (parts.length > 1) ? parts[1].strip.to!size_t : 0;
                    // consume empty line
                    auto _ = pipes.stdout.readln();
                    auto buf = new char[](len);
                    pipes.stdout.rawRead(buf);
                    pr = decodeJson(cast(string)buf);
                } else {
                    pr = decodeJson(line);
                }
                if (pr.hasValue) {
                    inboundMutex.lock();
                    inboundQueue ~= pr;
                    inboundCond.notify();
                    inboundMutex.unlock();
                }
            } catch (Exception e) {
                log("reader exit: "~e.msg);
                break;
            }
        }
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
        if (pipes.stderr.isOpen()) {
            while (!pipes.stderr.eof) {
                try {
                    auto ln = pipes.stderr.readln();
                    output ~= ln;
                    if (debugEchoPipes) log("[stderr] "~ln);
                } catch (StdioException) {
                    break;
                }
            }
        }
        return output;
    }

    void stderrLoop() {
        while (stderrRunning) {
            try {
                auto ln = pipes.stderr.readln();
                if (ln.length == 0) continue;
                log("[stderr] "~ln.strip());
            } catch (Exception) {
                break;
            }
        }
    }
}
