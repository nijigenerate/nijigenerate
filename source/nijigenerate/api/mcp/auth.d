module nijigenerate.api.mcp.auth;

import bindbc.imgui;
import nijigenerate.api.mcp.task;
import nijigenerate.widgets.notification;
import nijigenerate.widgets.button;
import std.string;
import core.time;
import std.datetime : SysTime, Clock, Duration;
import i18n;
import core.thread;
import vibe.core.core : sleep;

// waiting time for approval
private Duration approvalTimeout = 120.seconds;

struct ApprovalRequest {
    string reqId;
    string clientId;
    string scopeId;
    string resource;
    string state;
    string redirectUri;
}

string ngSimpleAuth(ApprovalRequest req) {
    import std.stdio;
    string decision;
    ulong pid = 0;
    ngRunInMainThread({
        writefln("[MCP/auth] popup");
        pid = NotificationPopup.instance().popup((ImGuiIO* io) {
            igText(_("Got authentication request for MCP server from %s (scope=%s), Do you approve it?").format(req.clientId, req.scopeId).toStringz);
            igSameLine();
            if (incButtonColored(__("Deny"))) {
                decision = "deny";
                NotificationPopup.instance().close(pid);
            }
            igSameLine();
            if (incButtonColored(__("Approve"))) {
                decision = "approve";
                NotificationPopup.instance().close(pid);
            }
        }, 120);
    });
    writefln("[MCP/auth] wait for close");
    // wait for decision.
    SysTime deadline = Clock.currTime() + approvalTimeout;
    while (Clock.currTime() < deadline) {
        if (decision.length) break;
        sleep(200.msecs);
    }
    ngRunInMainThread({ NotificationPopup.instance().close(pid); });
    writefln("[MCP/auth] auth done");
    return decision;
}
