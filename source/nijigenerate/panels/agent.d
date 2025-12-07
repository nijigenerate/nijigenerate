/*
    Coding Agent control panel (ACP)
    - Launch a Coding Agent executable with stdio pipes (ACPClient)
    - Initialize/Ping via ACP
    - Show log output
    Simplified: single worker thread, no fibers, no self-pipe.
*/
module nijigenerate.panels.agent;

import std.string : format, toStringz, join;
import std.array : appender;
import std.file : getcwd, exists;
import std.path : isAbsolute, buildPath;
import std.conv : to;
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
    bool initInFlight;
    MonoTime initStart;

    Thread worker;
    bool workerRunning;
    string[] cmdQueue;
    ACPResult[] resultQueue;
    Mutex qMutex;
    Condition qCond;

    void log(string msg) {
        logLines ~= msg;
        logBuffer = logLines.join("\n");
    }

    void enqueueUserText() {
        if (userText.length == 0) return;
        enqueueCommand("msg:" ~ userText);
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
                        localClient.sendPrompt(text);
                        pushResult("log", false, "prompt: "~text);
                    } catch (Exception e) {
                        pushResult("log", true, "send failed: "~e.msg);
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
                if (pr.error.length)
                    pushResult("log", true, pr.error);
                else
                    pushResult("log", false, pr.result.toString());
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
        auto cmd = parseCommandLine(agentPath);
        if (cmd.length == 0) {
            statusText = "Command is empty";
            log(statusText);
            return;
        }
        if (cmd.length > 0 && !isAbsolute(cmd[0])) {
            auto base = workingDir.length ? workingDir : getcwd();
            cmd[0] = buildPath(base, cmd[0]);
        }
        if (!exists(cmd[0])) {
            statusText = "Executable not found";
            log("not found: "~cmd[0]);
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
        } catch (Exception e) {
            statusText = "Start failed";
            log("error: " ~ e.toString());
            workerRunning = false;
        }
    }

    void pingAgent() {
        enqueueCommand("ping");
    }

    void stopAgent() {
        stopWorker();
        statusText = "Stopped";
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
    }

protected:
    override void onUpdate() {
        ImVec2 avail = incAvailableSpace();

        igText(_("Coding Agent executable:").toStringz());
        incInputText("##acp_agent_path", avail.x, agentPath);

        igText(_("Working directory (optional):").toStringz());
        incInputText("##acp_agent_cwd", avail.x, workingDir);

        if (igButton(_("Start").toStringz(), ImVec2(80, 0))) startAgent();
        igSameLine();
        if (igButton(_("Ping").toStringz(), ImVec2(80, 0))) pingAgent();
        igSameLine();
        if (igButton(_("Stop").toStringz(), ImVec2(80, 0))) stopAgent();
        igSameLine();
        if (igButton(_("Send Text").toStringz(), ImVec2(100, 0))) enqueueUserText();

        igSeparator();
        igText(_("Message to agent:").toStringz());
        incInputText("##acp_user_text", avail.x, userText);
        igSeparator();
        igText(_("Status: %s").format(statusText).toStringz());
        igSeparator();

        // handle results from worker
        drainResults();

        igText(_("Session output / logs:").toStringz());

        ImVec2 availLog = incAvailableSpace();
        float inputAreaH = igGetFrameHeightWithSpacing() * 2;
        ImVec2 logSize = ImVec2(0, availLog.y - inputAreaH);
        if (logSize.y < 80) logSize.y = availLog.y * 0.7f;

        if (igBeginChild("AgentLog", logSize, true,
                ImGuiWindowFlags.AlwaysVerticalScrollbar)) {
            auto flags = ImGuiInputTextFlags.ReadOnly
                       | ImGuiInputTextFlags.NoHorizontalScroll
                       | ImGuiInputTextFlags.AllowTabInput;
            incInputTextMultiline("##AgentLog", logBuffer, logSize, flags);
        }
        igEndChild();

        igSeparator();
        igText(_("User message:").toStringz());
        ImVec2 availBottom = incAvailableSpace();
        float sendBtnW = 80;
        float textW = availBottom.x - sendBtnW - igGetStyle().ItemSpacing.x;
        if (textW < 100) textW = availBottom.x * 0.7f;
        incInputText("##acp_user_text", textW, userText);
        igSameLine();
        if (igButton(_("Send").toStringz(), ImVec2(sendBtnW, 0))) {
            if (userText.length) {
                enqueueCommand("msg:" ~ userText);
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
