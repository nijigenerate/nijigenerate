/*
    Coding Agent control panel (ACP)
    - Launch a Coding Agent executable with stdio pipes (ACPClient)
    - Initialize/Ping via ACP
    - Show log output
    Simplified: single worker thread, no fibers, no self-pipe.
*/
module nijigenerate.panels.agent;

import std.string : format, toStringz, join;
import std.stdio : writeln;
import std.array : appender;
import std.file : getcwd, exists;
import std.path : isAbsolute, buildPath;
import std.conv : to;
import std.algorithm.searching : countUntil;
import core.thread;
import core.sync.mutex;
import core.sync.condition;
import core.time : msecs, MonoTime, seconds;
import std.process : ProcessException;
import std.json;
import bindbc.imgui;
import i18n;

import nijigenerate.panels;
import nijigenerate.widgets;
import nijigenerate.api.acp;
import nijigenerate.core.settings : incSettingsGet, incSettingsSet, incSettingsSave;

struct ACPResult {
    string kind;   // init, ping, log, error
    bool isError;
    string message;
}

class AgentPanel : Panel {
private:
    string agentPath = "./out/nijigenerate-agent";
    string workingDir;
    string statusText = "Idle";
    string[] logLines;
    string logBuffer;
    string userText;
    struct LineHeightCache {
        float wrapW = 0;
        float[] heights;
        size_t[] lens;    // cached line lengths to detect edits without length change
        uint dataVer = uint.max;
        float totalHeight = 0;
    }
    struct LogBlock {
        string role;
        string[] lines;
        LineHeightCache cache;
        uint ver;
    }
    LogBlock[] convoBlocks;     // committed blocks in order
    string pendingRole;
    ubyte[] pendingBuf; // partial bytes for current role
    bool pendingNewline; // true when previous chunk ended with '\n' → next chunk starts a new line
    bool hasPendingRole;
    bool convoContentAdded;
    bool logContentAdded;
    bool promptPending;
    uint logVersion;

    string safeDecode(const(ubyte)[] bytes) {
        import std.utf : decode, UTFException;
        auto app = appender!string();
        size_t i = 0;
        while (i < bytes.length) {
            try {
                auto chars = cast(const(char)[]) bytes;
                dchar ch = decode(chars, i);
                app.put(ch);
            } catch (UTFException) {
                app.put("\uFFFD");
                ++i; // skip one byte and continue
            }
        }
        return app.data;
    }
    bool initInFlight;
    MonoTime initStart;
    bool autoStartDone;
    MonoTime lastStartAttempt;

    Thread worker;
    bool workerRunning;
    string[] cmdQueue;
    ACPResult[] resultQueue;
    Mutex qMutex;
    Condition qCond;

    void log(string msg) {
        pushLogLine(shrinkLineLog(msg));
        logBuffer = logLines.join("\n");
    }

    enum size_t kMaxChatLineChars = 200; // reasonable chunk; rely on precise height clipping to avoid overflow
    enum size_t kMaxLogLineChars  = 200;  // logs stay short to protect DrawList
    enum size_t kMaxLines      = 4000; // cap to avoid huge draw lists
    enum size_t kMaxDrawCharsPerLine = 4000; // safety cap to avoid ImGui 16-bit index overflow
    LineHeightCache logCache;

    void renderClippedLines(const(string)[] lines, float wrapWidth, uint dataVer, ref LineHeightCache cache) {
        // Variable-height manual culling using cached wrapped heights.
        ImVec2 avail;
        igGetContentRegionAvail(&avail);
        float wrapW = (wrapWidth > 0) ? wrapWidth : avail.x;
        float cursorX = igGetCursorPosX();
        float baseY   = igGetCursorPosY();
        igPushTextWrapPos(cursorX + wrapW);

        auto style = igGetStyle();
        const float topWin    = igGetScrollY();
        const float bottomWin = topWin + igGetWindowHeight();
        const float relTop    = topWin - baseY;
        const float relBottom = bottomWin - baseY;

        bool wrapChanged   = cache.wrapW != wrapW;
        bool lengthChanged = cache.heights.length != lines.length;
        bool versionChanged = cache.dataVer != dataVer;

        auto drawableText = (string line) {
            if (line.length > kMaxDrawCharsPerLine)
                return line[0 .. kMaxDrawCharsPerLine] ~ " ... (display truncated)";
            return line;
        };

        if (wrapChanged || lengthChanged) {
            cache.heights.length = 0;
            cache.lens.length = 0;
            cache.totalHeight = 0;
            foreach (line; lines) {
                auto drawLine = drawableText(line);
                ImVec2 sz;
                igCalcTextSize(&sz, drawLine.toStringz(), null, true, wrapW);
                float h = sz.y + style.ItemSpacing.y;
                cache.heights ~= h;
                cache.lens ~= drawLine.length;
                cache.totalHeight += h;
            }
            cache.wrapW = wrapW;
            cache.dataVer = dataVer;
        } else if (versionChanged) {
            cache.totalHeight = 0;
            foreach (i, line; lines) {
                auto drawLine = drawableText(line);
                if (i >= cache.lens.length || cache.lens[i] != drawLine.length) {
                    ImVec2 sz;
                    igCalcTextSize(&sz, drawLine.toStringz(), null, true, wrapW);
                    float h = sz.y + style.ItemSpacing.y;
                    cache.heights[i] = h;
                    cache.lens[i] = drawLine.length;
                }
                cache.totalHeight += cache.heights[i];
            }
            cache.dataVer = dataVer;
        } else {
            // 完全ヒット: 再計算不要
        }

        float y = 0;
        foreach (i, line; lines) {
            float h = cache.heights[i];
            bool visible = (y + h >= relTop) && (y <= relBottom);
            if (visible) {
                igSetCursorPosY(baseY + y);
                string toDraw = line;
                if (toDraw.length > kMaxDrawCharsPerLine) {
                    toDraw = toDraw[0 .. kMaxDrawCharsPerLine] ~ " ... (display truncated)";
                }
                igTextUnformatted(toDraw.toStringz());
            }
            y += h;
        }
        // Advance cursor so scrollbar size matches total content.
        igSetCursorPosY(baseY + cache.totalHeight);
        igPopTextWrapPos();
    }

    string[] expandLines(const string[] src) {
        auto buf = appender!(string[])();
        foreach (line; src) {
            size_t start = 0;
            foreach (idx, ch; line) {
                if (ch == '\n') {
                    buf.put(line[start .. idx]);
                    start = idx + 1;
                }
            }
            buf.put(line[start .. line.length]);
        }
        return buf.data;
    }
    string[] splitLines(string s) {
        auto buf = appender!(string[])();
        size_t start = 0;
        foreach (idx, ch; s) {
            if (ch == '\n') {
                buf.put(s[start .. idx]);
                start = idx + 1;
            }
        }
        buf.put(s[start .. $]);
        return buf.data;
    }
    string shrinkLine(string s, size_t limit) {
        if (s.length <= limit) return s;
        auto head = s[0 .. limit];
        auto rest = s.length - limit;
        return head ~ " ... (truncated " ~ rest.to!string ~ " chars)";
    }
    string shrinkLineChat(string s) { return shrinkLine(s, kMaxChatLineChars); }
    string shrinkLineLog(string s)  { return shrinkLine(s, kMaxLogLineChars); }

    void ensureBlock(string role) {
        if (convoBlocks.length == 0 || convoBlocks[$-1].role != role) {
            LogBlock blk;
            blk.role = role;
            blk.ver = 0;
            convoBlocks ~= blk;
        }
    }

    void pushChatLine(string role, string line) {
        ensureBlock(role);
        auto blk = &convoBlocks[$-1];
        blk.lines ~= line;
        blk.ver++;
        convoContentAdded = true;
        // cap blocks by trimming oldest lines if overall too large
        // simple cap: if total lines exceed kMaxLines drop from front
        size_t total = 0;
        foreach (b; convoBlocks) total += b.lines.length;
        if (total > kMaxLines) {
            size_t toDrop = total - kMaxLines;
            size_t bi = 0;
            while (toDrop > 0 && bi < convoBlocks.length) {
                auto n = convoBlocks[bi].lines.length;
                if (n <= toDrop) {
                    toDrop -= n;
                    convoBlocks = convoBlocks[bi + 1 .. $];
                    bi = 0;
                } else {
                    convoBlocks[bi].lines = convoBlocks[bi].lines[toDrop .. $].dup;
                    toDrop = 0;
                }
            }
        }
    }

    void appendToLastLine(string role, string fragment) {
        ensureBlock(role);
        auto blk = &convoBlocks[$-1];
        if (blk.lines.length == 0) {
            blk.lines ~= fragment;
        } else {
            blk.lines[$-1] ~= fragment;
        }
        blk.ver++;
        convoContentAdded = true;
    }

    void pushLogLine(string line) {
        logLines ~= shrinkLineLog(line);
        logContentAdded = true;
        ++logVersion;
        if (logLines.length > kMaxLines) {
            auto drop = logLines.length - kMaxLines;
            logLines = logLines[drop .. $].dup;
        }
    }

    void appendStream(string role, string text) {
        if (hasPendingRole && role != pendingRole) {
            flushPending();
        }
        pendingRole = role;
        hasPendingRole = true;
        auto combined = pendingBuf ~ cast(const(ubyte)[]) text;
        size_t start = 0;
        bool startNewLine = pendingNewline;
        pendingNewline = false;

        while (true) {
            auto rel = countUntil(combined[start .. $], cast(ubyte) '\n');
            if (rel < 0) {
                auto slice = combined[start .. $];
                if (slice.length) {
                    auto dec = safeDecode(slice);
                    if (startNewLine || start > 0) {
                        pushChatLine(role, dec);
                    } else {
                        appendToLastLine(role, dec);
                    }
                }
                pendingBuf = null;
                hasPendingRole = false;
                return;
            }
            auto idx = start + rel;
            auto lineBytes = combined[start .. idx];
            auto lineDec = safeDecode(lineBytes);
            if (!startNewLine && start == 0 && convoBlocks.length > 0 && convoBlocks[$-1].role == role && convoBlocks[$-1].lines.length > 0) {
                // first segment of this chunk extends current last line
                appendToLastLine(role, lineDec);
            } else {
                pushChatLine(role, lineDec);
            }
            start = idx + 1;
            startNewLine = false;
            if (start == combined.length) {
                pendingNewline = true; // ended exactly on newline → next chunk starts fresh line
                pendingBuf = null;
                hasPendingRole = true;
                return;
            }
        }
    }

    void flushPending() {
        if (!hasPendingRole || (pendingBuf.length == 0 && !pendingNewline)) {
            hasPendingRole = false;
            pendingBuf = null;
            pendingNewline = false;
            return;
        }
        if (pendingNewline && pendingBuf.length == 0) {
            // dangling newline only – nothing to append, just reset state
            pendingNewline = false;
            hasPendingRole = false;
            return;
        }
        auto decoded = safeDecode(pendingBuf);
        if (pendingNewline) {
            pushChatLine(pendingRole, decoded);
        } else {
            appendToLastLine(pendingRole, decoded);
        }
        pendingBuf = null;
        hasPendingRole = false;
        pendingNewline = false;
    }

    void enqueueUserText() {
        if (userText.length == 0) return;
        enqueueCommand("msg:" ~ userText);
        promptPending = true;
        userText = "";
    }
    void enqueueCommand(string cmd) {
        qMutex.lock();
        cmdQueue ~= cmd;
        qMutex.unlock();
        signalWake();
    }

    void drainResults() {
        qMutex.lock();
        auto results = resultQueue.dup;
        resultQueue.length = 0;
        qMutex.unlock();

            foreach (res; results) {
                if (res.kind == "log" && !res.isError) {
                    log(res.message);
                    continue;
                }
                if (res.isError) {
                    statusText = res.message;
                    log(res.message);
                    if (res.kind == "init") initInFlight = false;
                } else if (res.kind == "init") {
                    statusText = "Initialized";
                    log("initialize: "~res.message);
                    initInFlight = false;
                } else if (res.kind == "ping") {
                    statusText = "Ping ok";
                    log("ping ok");
                } else if (res.kind == "promptDone") {
                    promptPending = false;
                } else {
                    log(res.message);
                }
            }
        }

    void pushResult(string kind, bool isError, string msg) {
        qMutex.lock();
        resultQueue ~= ACPResult(kind, isError, msg);
        qMutex.unlock();
    }

    void signalWake() {
        qCond.notifyAll();
    }

    void handleInbound(JSONValue obj) {
        if (obj.type == JSONType.object && "method" in obj.object) {
            auto m = obj["method"].str;
            if (m == "session/update") {
                auto params = obj["params"];
                if ("update" in params.object) {
                    auto upd = params["update"];
                    string kind = "session/update";
                    if ("sessionUpdate" in upd.object) kind = upd["sessionUpdate"].str;

                    string label = kind;
                    if (kind == "agent_message_chunk") label = "Assistant";
                    else if (kind == "agent_thought_chunk") label = "Assistant (thinking)";
                    else if (kind == "user_message_chunk") label = "You (echo)";
                    else if (kind == "plan") label = "Plan";
                    else if (kind == "tool_call" || kind == "tool_call_update") label = "Tool";
                    else if (kind == "available_commands_update") label = "Command list";
                    else if (kind == "current_mode_update") label = "Mode";

                    string text;
                    string status;
                    if ("status" in upd.object && upd["status"].type == JSONType.string) {
                        status = upd["status"].str;
                    }
                    string title;
                    if ("title" in upd.object && upd["title"].type == JSONType.string) {
                        title = upd["title"].str;
                    }
                    if ("content" in upd.object) {
                        auto content = upd["content"];
                        if (content.type == JSONType.object && "text" in content.object) {
                            text = content["text"].str;
                        } else if (content.type == JSONType.array && content.array.length) {
                            foreach (item; content.array) {
                                if (item.type == JSONType.object && "text" in item.object) {
                                    text ~= item["text"].str;
                                    if ("annotations" in item.object && item["annotations"].type == JSONType.array) {
                                        foreach (ann; item["annotations"].array) {
                                            if (ann.type == JSONType.object && "title" in ann.object) {
                                                text ~= " (" ~ ann["title"].str ~ ")";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (text.length == 0 && "message" in upd.object) {
                        text = upd["message"].toString();
                    }
                    if (text.length == 0 && "rawOutput" in upd.object) {
                        text = upd["rawOutput"].toString();
                    }
                    if (text.length == 0) {
                        // fallback: whole update JSON
                        text = upd.toString();
                    }
                    string summary = text;
                    if (kind == "tool_call" || kind == "tool_call_update") {
                        if (title.length) summary = title ~ " " ~ summary;
                        if (status.length) summary ~= " (status: " ~ status ~ ")";
                    }
                    appendStream(label, summary);
                }
            }
            pushResult("log", false, obj.toString());
            return;
        }

        if (obj.type == JSONType.object && "id" in obj.object) {
            // response: finalize any streaming buffers into history
            flushPending();
            promptPending = false;
            pushResult("log", false, obj.toString());
            return;
        }

        pushResult("log", false, obj.toString());
    }

    void workerLoop(string[] cmd, string wd) {
        ACPClient localClient;
        try {
            localClient = new ACPClient(cmd, wd.length ? wd : null);
            localClient.setLogger((msg) => pushResult("log", false, msg));
            localClient.setCancelCheck(() => !workerRunning);
            // Codex ACP expects line-delimited JSON; start with line mode
            localClient.setSendMode("line");
            pushResult("log", false, "spawned pid: "~localClient.getPid().to!string);
        } catch (Exception e) {
            pushResult("init", true, "spawn failed: "~e.msg);
            return;
        }

        while (workerRunning) {
            // fetch commands
            string[] cmds;
            qMutex.lock();
            cmds = cmdQueue.dup;
            cmdQueue.length = 0;
            qMutex.unlock();

            foreach (c; cmds) {
                if (c == "init") {
                    localClient.initializeAsync();
                    initInFlight = true;
                    initStart = MonoTime.currTime;
                } else if (c == "ping") {
                    try {
                        localClient.ping();
                        pushResult("ping", false, "");
                    } catch (Exception e) {
                        pushResult("ping", true, e.msg);
                    }
                } else if (c.startsWith("msg:")) {
                    auto text = c[4 .. $];
                    try {
                        appendStream("You", text);
                        localClient.sendPrompt(text);
                        pushResult("log", false, "prompt: "~text);
                    } catch (Exception e) {
                        pushResult("log", true, "send failed: "~e.msg);
                    }
                } else if (c == "cancel") {
                    try {
                        localClient.cancelPrompt();
                        pushResult("promptDone", false, "cancel sent");
                    } catch (Exception e) {
                        pushResult("log", true, "cancel failed: "~e.msg);
                    }
                }
            }

            // drain inbound messages from client
            foreach (pr; localClient.takeInbound()) {
                if (initInFlight) {
                    initInFlight = false;
                    if (pr.error.length)
                        pushResult("init", true, pr.error);
                    else
                        pushResult("init", false, pr.result.toString());
                    continue;
                }
                if (pr.error.length) {
                    pushResult("log", true, pr.error);
                    continue;
                }
                // handle notifications/responses
                auto obj = pr.result;
                handleInbound(obj);
            }

            // if still waiting for init and nothing arrived, check once
            if (initInFlight) {
                auto done = localClient.pollInitialize();
                if (done) {
                    initInFlight = false;
                    if (localClient.getInitError().length)
                        pushResult("init", true, localClient.getInitError());
                    else
                        pushResult("init", false, localClient.getInitResult().toString());
                }
            }

            // wait for next event or stop request
            qMutex.lock();
            if (cmdQueue.length == 0 && workerRunning) {
                qCond.wait(100.msecs);
            }
            qMutex.unlock();
        }

        if (localClient !is null) localClient.close();
    }

    void stopWorker() {
        if (worker !is null) {
            workerRunning = false;
            signalWake();
            worker.join();
            worker = null;
        }
    }

    void startAgent() {
        stopAgent(); // ensure previous process closed
        convoBlocks.length = 0;
        pendingBuf = null;
        pendingNewline = false;
        hasPendingRole = false;
        auto cmd = parseCommandLine(agentPath);
        if (cmd.length == 0) {
            statusText = "Command is empty";
            log(statusText);
            lastStartAttempt = MonoTime.currTime;
            return;
        }
        if (cmd.length > 0 && !isAbsolute(cmd[0])) {
            auto base = workingDir.length ? workingDir : getcwd();
            cmd[0] = buildPath(base, cmd[0]);
        }
        if (!exists(cmd[0])) {
            statusText = "Executable not found";
            log("not found: "~cmd[0]);
            lastStartAttempt = MonoTime.currTime;
            return;
        }
        try {
            incSettingsSet("ACP.Command", agentPath);
            incSettingsSet("ACP.Workdir", workingDir);
            incSettingsSave();
            workerRunning = true;
            auto cmdCopy = cmd.dup;
            auto wdCopy = workingDir;
            worker = new Thread(() => workerLoop(cmdCopy, wdCopy));
            worker.isDaemon(true); // allow process exit fallback even if not joined
            worker.start();
            enqueueCommand("init");
            initInFlight = true;
            statusText = "Initializing...";
            log("worker started");
            autoStartDone = true;
        } catch (Exception e) {
            statusText = "Start failed";
            log("error: " ~ e.toString());
            workerRunning = false;
            lastStartAttempt = MonoTime.currTime;
        }
    }

    void pingAgent() {
        enqueueCommand("ping");
    }

    void stopAgent() {
        stopWorker();
        statusText = "Stopped";
        convoBlocks.length = 0;
        pendingBuf = null;
        pendingNewline = false;
        hasPendingRole = false;
    }

    /// Very small command-line tokenizer (handles quotes, no escapes)
    string[] parseCommandLine(string line) {
        enum State { normal, inSingle, inDouble }
        auto result = appender!(string[])();
        string cur;
        State st = State.normal;
        foreach (ch; line) {
            final switch (st) {
                case State.normal:
                    if (ch == ' ' || ch == '\t') {
                        if (cur.length) { result.put(cur); cur = ""; }
                    } else if (ch == '"') st = State.inDouble;
                    else if (ch == '\'') st = State.inSingle;
                    else cur ~= ch;
                    break;
                case State.inSingle:
                    if (ch == '\'') st = State.normal;
                    else cur ~= ch;
                    break;
                case State.inDouble:
                    if (ch == '"') st = State.normal;
                    else cur ~= ch;
                    break;
            }
        }
        if (cur.length) result.put(cur);
        return result.data;
    }

public:
    this() {
        super("Agent", _("Agent"), false);
    }

    ~this() {
        stopWorker();
    }

    override void onInit() {
        qMutex = new Mutex();
        qCond = new Condition(qMutex);
        agentPath = incSettingsGet!string("ACP.Command", agentPath);
        workingDir = incSettingsGet!string("ACP.Workdir", workingDir);
        lastStartAttempt = MonoTime.currTime;
    }

protected:
    override void onUpdate() {
        ImVec2 avail = incAvailableSpace();

        // auto-start once when possible (cooldown 1s between attempts)
        auto now = MonoTime.currTime;
        if (!workerRunning && !initInFlight) {
            auto delta = now - lastStartAttempt;
            if (delta > seconds(1)) {
                startAgent();
            }
        }

        igSeparator();
        igText(_("Status: %s").format(statusText).toStringz());
        igSeparator();

        // handle results from worker
        drainResults();

        ImVec2 availLog = incAvailableSpace();
        float inputAreaH = igGetFrameHeightWithSpacing() * 2   // status + separator padding
                         + igGetStyle().ItemSpacing.y * 3      // tab bar + spacing
                         + igGetFrameHeightWithSpacing();      // user message line height
        ImVec2 logSize = ImVec2(availLog.x, availLog.y - inputAreaH);
        if (logSize.y < 160) logSize.y = availLog.y * 0.65f;

        // Conversation / Log tabs
        if (igBeginTabBar("AgentTabs", ImGuiTabBarFlags.None)) {
            if (igBeginTabItem(_("Conversation").toStringz())) {
                flushPending(); // show latest partial lines
                if (igBeginChild("AgentConversation", logSize, true,
                        ImGuiWindowFlags.AlwaysVerticalScrollbar)) {
                    foreach (i, blk; convoBlocks) {
                        auto label = (blk.role ~ "##blk" ~ i.to!string()).toStringz();
                        auto flags = ImGuiTreeNodeFlags.None;
                        // 最終ブロックは開く。ユーザ(You)ブロックも常に開く。
                        if (i == convoBlocks.length - 1 || blk.role == "You") {
                            flags |= ImGuiTreeNodeFlags.DefaultOpen;
                        }
                        if (igTreeNodeEx(label, flags)) {
                            renderClippedLines(blk.lines, logSize.x, blk.ver, blk.cache);
                            igTreePop();
                        }
                    }
                }
                igEndChild();
                convoContentAdded = false;
                igEndTabItem();
            }
            if (igBeginTabItem(_("Log").toStringz())) {
                auto linesLog = expandLines(logLines);
                if (igBeginChild("AgentLog", logSize, true,
                        ImGuiWindowFlags.AlwaysVerticalScrollbar)) {
                    renderClippedLines(linesLog, logSize.x, logVersion, logCache);
                }
                igEndChild();
                logContentAdded = false;
                igEndTabItem();
            }
            igEndTabBar();
        }

        igSeparator();
        igText(_("User message:").toStringz());
        float sendBtnW = 80;
        float textW = availLog.x - sendBtnW - igGetStyle().ItemSpacing.x;
        if (textW < 160) textW = availLog.x * 0.75f;
        bool submit = incInputText("##acp_user_text", textW, userText, ImGuiInputTextFlags.EnterReturnsTrue);
        if (submit && userText.length) {
            enqueueUserText();
        }
        igSameLine();
        if (!promptPending) {
            if (incButtonColored(_("\ue037").toStringz(), ImVec2(sendBtnW, 0))) {
                if (userText.length) {
                    enqueueUserText();
                }
            }
        } else {
            if (incButtonColored(_("\ue047").toStringz(), ImVec2(sendBtnW, 0))) {
                enqueueCommand("cancel");
            }
        }
    }
}

mixin incPanel!AgentPanel;

/// Global helper to stop ACP worker if the panel exists.
void ngAcpStopAll() {
    auto p = incFindPanelByName("Agent");
    if (p !is null) {
        (cast(AgentPanel)p).stopAgent();
    }
}

/// Final safety: ensure ACP worker stops on process exit even if app loop skips explicit stop.
shared static ~this() {
    ngAcpStopAll();
}
