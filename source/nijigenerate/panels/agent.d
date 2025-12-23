/*
    Coding Agent control panel (ACP)
    - Launch a Coding Agent executable with stdio pipes (ACPClient)
    - Initialize/Ping via ACP
    - Show log output
    Simplified: single worker thread, no fibers, no self-pipe.
*/
module nijigenerate.panels.agent;

import std.string : format, toStringz, join, split;
import std.stdio : writeln;
import std.array : appender, array, replicate;
import std.digest.crc : crc32Of;
import std.file : getcwd, exists;
import std.path : isAbsolute, buildPath;
import std.conv : to;
import std.algorithm.searching : countUntil, startsWith;
import core.thread;
import core.sync.mutex;
import core.sync.condition;
import core.time : msecs, MonoTime, seconds;
import std.process : ProcessException;
import std.json;
import std.algorithm : min;
import std.string : strip, indexOf;
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

struct PermissionRequest {
    string id;
    string reason;
    JSONValue params;
    bool responded;
    bool granted;
}

class AgentPanel : Panel {
private:
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
        string title;         // optional (tool calls)
        string[] lines;
        JSONValue[] payloads; // legacy per-line payloads (kept for non-tool)
        JSONValue payloadMerged; // for tool_call/_update
        bool hasPayloadMerged;
        string status;        // tool status
        LineHeightCache cache;
        uint ver;
        size_t dirtyFrom = size_t.max; // first line index that needs height recompute
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

    void clearState() {
        convoBlocks.length = 0;
        logLines.length = 0;
        logBuffer = "";
        logCache = LineHeightCache.init;
        logVersion = 0;
        pendingBuf = null;
        pendingNewline = false;
        hasPendingRole = false;
        convoContentAdded = false;
        logContentAdded = false;
    }

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
    PermissionRequest[] permQueue;
    bool permChanged = false;

    void log(string msg) {
        pushLogLine(shrinkLineLog(msg));
        logBuffer = logLines.join("\n");
    }

    enum size_t kMaxChatLineChars = 200; // reasonable chunk; rely on precise height clipping to avoid overflow
    enum size_t kMaxLogLineChars  = 2000;  // logs stay short to protect DrawList
    enum size_t kMaxLines      = 4000; // cap to avoid huge draw lists
    enum size_t kMaxDrawCharsPerLine = 4000; // safety cap to avoid ImGui 16-bit index overflow
    LineHeightCache logCache;

    void renderClippedLines(const(string)[] lines, float wrapWidth, uint dataVer, ref LineHeightCache cache,
            bool prefixFirstOnly = false, string prefix = "", ImVec4* optColor = null,
            size_t dirtyFrom = size_t.max, size_t startIdx = 0) {
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

        auto padPrefix = prefix.length ? replicate(" ", prefix.length) : "";
        auto displayLine = (size_t idx) {
            if (!prefix.length) return lines[idx];
            if (prefixFirstOnly) {
                return (idx == 0 ? prefix : padPrefix) ~ lines[idx];
            }
            return prefix ~ lines[idx];
        };

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
            foreach (idx, line; lines) {
                auto disp = displayLine(idx);
                auto drawLine = drawableText(disp);
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
            // length shrink -> recompute (rare)
            if (lengthChanged && lines.length < cache.heights.length) {
                cache.heights.length = 0;
                cache.lens.length = 0;
                cache.totalHeight = 0;
                foreach (idx, line; lines) {
                    auto disp = displayLine(idx);
                    auto drawLine = drawableText(disp);
                    ImVec2 sz;
                    igCalcTextSize(&sz, drawLine.toStringz(), null, true, wrapW);
                    float h = sz.y + style.ItemSpacing.y;
                    cache.heights ~= h;
                    cache.lens ~= drawLine.length;
                    cache.totalHeight += h;
                }
                cache.wrapW = wrapW;
                cache.dataVer = dataVer;
            } else {
                // length grow (append) -> add only new lines
                if (lengthChanged && lines.length > cache.heights.length) {
                    auto start = cache.heights.length;
                    cache.heights.length = lines.length;
                    cache.lens.length = lines.length;
                    foreach (i; start .. lines.length) {
                        auto disp = displayLine(i);
                        auto drawLine = drawableText(disp);
                        ImVec2 sz;
                        igCalcTextSize(&sz, drawLine.toStringz(), null, true, wrapW);
                        float h = sz.y + style.ItemSpacing.y;
                        cache.heights[i] = h;
                        cache.lens[i] = drawLine.length;
                        cache.totalHeight += h;
                    }
                }
                // Recompute only the updated range
                if (dirtyFrom != size_t.max && dirtyFrom < lines.length) {
                    foreach (i; dirtyFrom .. lines.length) {
                        auto disp = displayLine(i);
                        auto drawLine = drawableText(disp);
                        ImVec2 sz;
                        igCalcTextSize(&sz, drawLine.toStringz(), null, true, wrapW);
                        float h = sz.y + style.ItemSpacing.y;
                        auto oldH = (i < cache.heights.length) ? cache.heights[i] : 0;
                        cache.heights[i] = h;
                        cache.lens[i] = drawLine.length;
                        cache.totalHeight += (h - oldH);
                    }
                }
                cache.wrapW = wrapW;
                cache.dataVer = dataVer;
            }
        } else {
            // Full hit: no recompute needed
        }

        float prefixH = 0;
        if (startIdx > 0 && startIdx <= cache.heights.length) {
            foreach (k; 0 .. startIdx) prefixH += cache.heights[k];
        }
        float y = 0;
        foreach (offset, line; lines[startIdx .. $]) {
            size_t i = startIdx + offset;
            float h = cache.heights[i];
            bool visible = (prefixH + y + h >= relTop) && (prefixH + y <= relBottom);
            if (visible) {
                igSetCursorPosY(baseY + prefixH + y);
                auto disp = displayLine(i);
                string toDraw = drawableText(disp);
                if (toDraw.length > kMaxDrawCharsPerLine) {
                    toDraw = toDraw[0 .. kMaxDrawCharsPerLine] ~ " ... (display truncated)";
                }
                if (optColor !is null) {
                    igTextColored(*optColor, toDraw.toStringz());
                } else {
                    igTextUnformatted(toDraw.toStringz());
                }
            }
            y += h;
            // All remaining lines are below; stop if past the window bottom
            if (prefixH + y > relBottom && !visible) break;
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

    string deriveUpdateKind(JSONValue upd) {
        if (upd.type == JSONType.object && "sessionUpdate" in upd.object)
            return upd["sessionUpdate"].str;
        return "session/update";
    }

    string deriveToolId(JSONValue upd) {
        if (upd.type == JSONType.object && "toolCallId" in upd.object
            && upd["toolCallId"].type == JSONType.string)
            return upd["toolCallId"].str;
        return "";
    }

    string deriveRoleFromKind(string kind, string toolId) {
        if (kind == "agent_message_chunk") return "Assistant";
        if (kind == "agent_thought_chunk") return "Assistant (thinking)";
        if (kind == "user_message_chunk") return "You (echo)";
        if (kind == "plan") return "Plan";
        if (kind == "error") return "Error";
        if (kind == "tool_call" || kind == "tool_call_update")
            return toolId.length ? "Tool#" ~ toolId : "Tool";
        if (kind == "available_commands_update") return "Command list";
        if (kind == "current_mode_update") return "Mode";
        return kind;
    }

    string extractTextFromContent(JSONValue upd) {
        if (upd.type != JSONType.object) return "";
        JSONValue content = JSONValue.init;
        if (upd.type == JSONType.object && "content" in upd.object)
            content = upd["content"];
        if (content.type == JSONType.object && "text" in content.object) {
            return content["text"].str;
        }
        if (content.type == JSONType.array && content.array.length) {
            string acc;
            foreach (item; content.array) {
                if (item.type == JSONType.object && "text" in item.object) {
                    acc ~= item["text"].str;
                    if ("annotations" in item.object && item["annotations"].type == JSONType.array) {
                        foreach (ann; item["annotations"].array) {
                            if (ann.type == JSONType.object && "title" in ann.object) {
                                acc ~= " (" ~ ann["title"].str ~ ")";
                            }
                        }
                    }
                }
            }
            return acc;
        }
        return "";
    }

    string fallbackText(JSONValue upd) {
        if (upd.type != JSONType.object) return upd.toString();
        if ("message" in upd.object && upd["message"].type == JSONType.string)
            return upd["message"].str;
        if ("rawOutput" in upd.object)
            return upd["rawOutput"].toString();
        return upd.toString();
    }

    void handleSessionUpdate(JSONValue upd) {
        string kind = deriveUpdateKind(upd);
        string toolId = deriveToolId(upd);
        string role = deriveRoleFromKind(kind, toolId);

        string status;
        if (upd.type == JSONType.object && "status" in upd.object
            && upd["status"].type == JSONType.string)
            status = upd["status"].str;

        string title;
        if (upd.type == JSONType.object && "title" in upd.object
            && upd["title"].type == JSONType.string)
            title = upd["title"].str;

        string text = extractTextFromContent(upd);

        bool isTool = (kind == "tool_call" || kind == "tool_call_update");
        bool isPlan = (kind == "plan");

        if (isTool) {
            if (text.length == 0 && "message" in upd.object && upd["message"].type == JSONType.string)
                text = upd["message"].str;
            if (text.length == 0) text = "tool update";
            if (title.length) text = title ~ " " ~ text;
            if (status.length) text ~= " (status: " ~ status ~ ")";
            if (toolId.length) text = "[" ~ toolId ~ "] " ~ text;

            bool isInitial = (kind == "tool_call");
            handleToolUpdate(role, title, text, upd, isInitial, status);
            return;
        }

        if (isPlan) {
            handlePlanUpdate(text, upd, status);
            return;
        }

        if (text.length == 0)
            text = fallbackText(upd);

        appendStream(role, text, upd, title, false, false);
    }

    string jsonScalar(JSONValue v) {
        switch (v.type) {
            case JSONType.string:   return "\"" ~ v.str ~ "\"";
            case JSONType.null_:    return "null";
            case JSONType.true_:    return "true";
            case JSONType.false_:   return "false";
            case JSONType.integer:  return v.integer.to!string;
            case JSONType.uinteger: return v.uinteger.to!string;
            case JSONType.float_:   return v.floating.to!string;
            default:                return v.toString();
        }
    }

    struct TrimView {
        size_t start;
        size_t end;
        bool trimmed;
    }

    TrimView trimBlankEdges(const(string)[] src) {
        size_t s = 0;
        size_t e = src.length;
        while (s < e && src[s].strip.length == 0) ++s;
        while (e > s && src[e - 1].strip.length == 0) --e;
        return TrimView(s, e, s > 0 || e < src.length);
    }

    string shortToolTitle(string title) {
        auto p = title.indexOf('\n');
        if (p >= 0) return title[0 .. p] ~ " ...";
        return title;
    }

    struct PlanView {
        string[] lines;
        JSONValue payload;
        string status;
        uint ver;
    }
    bool hasPlan;
    PlanView planView;

    enum RoleKind { You, Tool, AssistantFinal, AssistantThinking, Error, Other }

    RoleKind roleKind(const LogBlock blk) {
        if (blk.role == "You") return RoleKind.You;
        if (blk.role.startsWith("Tool")) return RoleKind.Tool;
        if (blk.role == "Assistant") return RoleKind.AssistantFinal;
        if (blk.role.startsWith("Assistant")) return RoleKind.AssistantThinking;
        if (blk.role == "Error") return RoleKind.Error;
        return RoleKind.Other;
    }

    void renderYou(ref LogBlock blk, float w, ImVec4 col) {
        auto tv = trimBlankEdges(blk.lines);
        auto view = blk.lines[tv.start .. tv.end];
        if (tv.trimmed) {
            LineHeightCache tmp;
            renderClippedLines(view, w, blk.ver, tmp, true, "\ue7fd: ", &col);
        } else {
            renderClippedLines(view, w, blk.ver, blk.cache, true, "\ue7fd: ", &col, blk.dirtyFrom);
            blk.dirtyFrom = size_t.max;
        }
    }

    void renderTool(ref LogBlock blk, float w, bool isLast) {
        ImVec4 c;
        igColorConvertU32ToFloat4(&c, statusColor(blk.status));
        auto labelVis = "tool: " ~ (blk.title.length ? shortToolTitle(blk.title) : blk.role);
        auto labelId = ("##blk" ~ (cast(size_t)&blk).to!string()).toStringz();
        auto flags = ImGuiTreeNodeFlags.None;
        if (isLast) flags |= ImGuiTreeNodeFlags.DefaultOpen;
        bool open = igTreeNodeEx(labelId, flags);
        float labelSpacing = igGetTreeNodeToLabelSpacing();
        igSameLine(0, labelSpacing);
        igTextColored(c, "\ue86c");
        igSameLine(0, 4);
        igTextUnformatted(labelVis.toStringz());
        if (open) {
            if (blk.hasPayloadMerged) {
                // Show tool input/output; include rawInput/rawOutput/content/contents
                string[] keys = ["rawInput", "rawOutput", "content", "contents"];
                bool shown = renderJsonFiltered(blk.payloadMerged, w, keys);
                // If nothing matched the filter, fall back to showing entire payload
                if (!shown) renderJsonTree(blk.payloadMerged, "", w, "", 0, null, true);
            }
            igTreePop();
        }
    }

    void renderAssistantThinking(ref LogBlock blk, float w) {
        string thinkingLabel;
        size_t thinkingFirstIdx = 0;
        bool found = false;
        foreach (idx, ln; blk.lines) {
            if (ln.strip.length) { thinkingLabel = ln; thinkingFirstIdx = idx; found = true; break; }
        }
        if (!found && blk.lines.length) thinkingLabel = blk.lines[0];
        if (!found && thinkingLabel.length == 0) thinkingLabel = blk.role;

        auto labelId = (thinkingLabel ~ "##blk" ~ (cast(size_t)&blk).to!string()).toStringz();
        if (igTreeNodeEx(labelId, ImGuiTreeNodeFlags.None)) {
            size_t startIdx = thinkingFirstIdx + 1;
            if (startIdx < blk.lines.length) {
                auto tvThink = trimBlankEdges(blk.lines[startIdx .. $]);
                auto viewThink = blk.lines[startIdx + tvThink.start .. startIdx + tvThink.end];
                LineHeightCache tmp;
                renderClippedLines(viewThink, w, blk.ver, tmp);
            }
            igTreePop();
        }
    }

    void renderAssistantFinal(ref LogBlock blk, float w, bool isLast) {
        auto labelId = (blk.role ~ "##blk" ~ (cast(size_t)&blk).to!string()).toStringz();
        auto catFlags = isLast ? IncCategoryFlags.None : IncCategoryFlags.DefaultClosed;
        bool opened = incBeginCategory(labelId, catFlags);
        if (opened) {
            auto tv = trimBlankEdges(blk.lines);
            auto view = blk.lines[tv.start .. tv.end];
            if (tv.trimmed) {
                LineHeightCache tmp;
                renderClippedLines(view, w, blk.ver, tmp);
            } else {
                renderClippedLines(view, w, blk.ver, blk.cache, false, "", null, blk.dirtyFrom);
                blk.dirtyFrom = size_t.max;
            }
        }
        incEndCategory();
    }

    void renderError(ref LogBlock blk, float w, bool isLast) {
        ImVec4 red = ImVec4(0.9f, 0.1f, 0.1f, 1);
        auto labelId = (blk.role ~ "##blk" ~ (cast(size_t)&blk).to!string()).toStringz();
        auto flags = ImGuiTreeNodeFlags.None;
        if (isLast) flags |= ImGuiTreeNodeFlags.DefaultOpen;
        if (igTreeNodeEx(labelId, flags)) {
            auto tv = trimBlankEdges(blk.lines);
            auto view = blk.lines[tv.start .. tv.end];
            if (tv.trimmed) {
                LineHeightCache tmp;
                renderClippedLines(view, w, blk.ver, tmp, false, "", &red);
            } else {
                renderClippedLines(view, w, blk.ver, blk.cache, false, "", &red, blk.dirtyFrom);
                blk.dirtyFrom = size_t.max;
            }
            igTreePop();
        }
    }

    void renderOther(ref LogBlock blk, float w, bool isLast) {
        auto labelId = (blk.role ~ "##blk" ~ (cast(size_t)&blk).to!string()).toStringz();
        auto flags = ImGuiTreeNodeFlags.None;
        if (isLast) flags |= ImGuiTreeNodeFlags.DefaultOpen;
        if (igTreeNodeEx(labelId, flags)) {
            auto tv = trimBlankEdges(blk.lines);
            auto view = blk.lines[tv.start .. tv.end];
            if (tv.trimmed) {
                LineHeightCache tmp;
                renderClippedLines(view, w, blk.ver, tmp);
            } else {
                renderClippedLines(view, w, blk.ver, blk.cache, false, "", null, blk.dirtyFrom);
                blk.dirtyFrom = size_t.max;
            }
            igTreePop();
        }
    }

    JSONValue mergeJson(JSONValue dst, JSONValue src) {
        /* merge rule (tool payload):
           - key=="contents": if both arrays → concat; otherwise append src to array (promote to array if needed)
           - key=="content":  if both object and have string "text" → text concat;
                              else if both arrays → concat;
                              else if dst array → append src;
                              else if src array and dst not array → prepend dst then concat;
                              else replace
           - other keys: replace
        */
        if (dst.type == JSONType.object && src.type == JSONType.object) {
            JSONValue res;
            res.object = dst.object.dup;
            foreach (k, v; src.object) {
                // contents
                if (k == "contents") {
                    if (k in res.object) {
                        auto d = res.object[k];
                        if (d.type == JSONType.array && v.type == JSONType.array) {
                            JSONValue merged;
                            merged.array = d.array ~ v.array;
                            res.object[k] = merged;
                        } else if (d.type == JSONType.array) {
                            JSONValue merged;
                            merged.array = d.array ~ [v];
                            res.object[k] = merged;
                        } else if (v.type == JSONType.array) {
                            JSONValue merged;
                            merged.array = [d] ~ v.array;
                            res.object[k] = merged;
                        } else {
                            // promote both to array
                            JSONValue merged;
                            merged.array = [d, v];
                            res.object[k] = merged;
                        }
                    } else {
                        res.object[k] = v;
                    }
                    continue;
                }
                // content
                if (k == "content") {
                    if (k in res.object) {
                        auto d = res.object[k];
                        // text concat
                        if (d.type == JSONType.object && v.type == JSONType.object
                            && "text" in d.object && "text" in v.object
                            && d["text"].type == JSONType.string && v["text"].type == JSONType.string) {
                            JSONValue merged = d;
                            merged.object["text"].str = d["text"].str ~ v["text"].str;
                            res.object[k] = merged;
                        } else {
                            // If not text, push into an array
                            JSONValue merged;
                            if (d.type == JSONType.array)
                                merged.array = d.array ~ [v];
                            else
                                merged.array = [d, v];
                            res.object[k] = merged;
                        }
                    } else {
                        res.object[k] = v;
                    }
                    continue;
                }
                // default: replace
                res.object[k] = v;
            }
            return res;
        }
        // non-object: replace
        return src;
    }

    ImU32 statusColor(string s) {
        ImVec4 col;
        switch (s) {
            case "in_progress": col = ImVec4(0, 0.4f, 0.8f, 1); break; // blue
            case "completed": col = ImVec4(0, 0.75f, 0, 1); break; // green
            case "failed": col = ImVec4(0.9f, 0, 0, 1); break; // red
            case "queued": col = ImVec4(0.9f, 0.5f, 0, 1); break; // orange
            case "cancelled": col = ImVec4(0.5f, 0.5f, 0.5f, 1); break; // gray
            default: col = ImVec4(0.6f, 0.6f, 0.6f, 1); break;
        }
        return igGetColorU32(col);
    }

    bool renderJsonFiltered(JSONValue v, float wrapW, string[] focusKeys) {
        bool shown = false;
        switch (v.type) {
            case JSONType.object: {
                size_t idx = 0;
                foreach (k, val; v.object) {
                    if (renderJsonTree(val, k, wrapW, "", idx++, focusKeys)) shown = true;
                }
                break;
            }
            case JSONType.array: {
                foreach (idx, val; v.array) {
                    if (renderJsonTree(val, "[" ~ idx.to!string ~ "]", wrapW, "", idx, focusKeys)) shown = true;
                }
                break;
            }
            default:
                // scalar with no label cannot match focus; ignore
                break;
        }
        return shown;
    }

    // focusKeys: when non-empty -> show only branches containing those keys.
    // allowAllBelow: when true, show descendants without filtering (under a key hit).
    bool renderJsonTree(JSONValue v, string label, float wrapW, string path = "", size_t siblingIdx = 0, string[] focusKeys = null, bool allowAllBelow = false) {
        string nextLabel(string lbl, string p) {
            auto pathNext = p.length ? (p ~ "/" ~ lbl) : lbl;
            auto h = crc32Of(pathNext);
            string vis = lbl.length ? lbl : "(root)";
            return (vis ~ "##json" ~ h.to!string);
        }
        auto id = nextLabel(label ~ "#" ~ siblingIdx.to!string, path);
        string nextPath = path.length ? (path ~ "/" ~ label ~ "#" ~ siblingIdx.to!string) : (label ~ "#" ~ siblingIdx.to!string);

        bool hasFocus = focusKeys !is null && focusKeys.length > 0;
        bool keyMatch(string k) {
            if (!hasFocus) return true;
            foreach (fk; focusKeys) if (k == fk) return true;
            return false;
        }
        bool containsKey(JSONValue vv) {
            switch (vv.type) {
                case JSONType.object:
                    foreach (k, val; vv.object) {
                        if (keyMatch(k)) return true;
                        if (containsKey(val)) return true;
                    }
                    return false;
                case JSONType.array:
                    foreach (val; vv.array) if (containsKey(val)) return true;
                    return false;
                default:
                    return false;
            }
        }

        switch (v.type) {
            case JSONType.object:
                bool matchedHere = keyMatch(label);
                if (!allowAllBelow && hasFocus && !matchedHere && !containsKey(v)) return false;
                if (igTreeNodeEx(id.toStringz(), ImGuiTreeNodeFlags.None)) {
                    size_t idx = 0;
                    foreach (k, val; v.object) {
                        bool childMatched = keyMatch(k);
                        bool childAllow = allowAllBelow || matchedHere || childMatched;
                        if (allowAllBelow || matchedHere || childMatched || !hasFocus || containsKey(val)) {
                            renderJsonTree(val, k, wrapW, nextPath, idx, focusKeys, childAllow);
                        }
                        ++idx;
                    }
                    igTreePop();
                }
                return true;
            case JSONType.array:
                if (!allowAllBelow && hasFocus && !containsKey(v)) return false;
                if (igTreeNodeEx(id.toStringz(), ImGuiTreeNodeFlags.None)) {
                    foreach (idx, val; v.array) {
                        bool childAllow = allowAllBelow;
                        if (allowAllBelow || !hasFocus || containsKey(val)) {
                            renderJsonTree(val, "[" ~ idx.to!string ~ "]", wrapW, nextPath, idx, focusKeys, childAllow);
                        }
                    }
                    igTreePop();
                }
                return true;
            default: {
                if (allowAllBelow || !hasFocus || keyMatch(label)) {
                    auto line = label.length ? (label ~ ": " ~ jsonScalar(v)) : jsonScalar(v);
                    LineHeightCache tmp;
                    renderClippedLines([line], wrapW, 0, tmp);
                    return true;
                } else {
                    return false;
                }
            }
        }
        return true; // fallback
    }

    void ensureBlock(string role, string title = "") {
        if (convoBlocks.length == 0 || convoBlocks[$-1].role != role) {
            LogBlock blk;
            blk.role = role;
            blk.title = title;
            blk.ver = 0;
            convoBlocks ~= blk;
        } else if (title.length) {
            // update title if provided later
            convoBlocks[$-1].title = title;
        }
    }

    void pushChatLine(string role, string line, JSONValue payload = JSONValue.init, string title = "", bool payloadPresent = false) {
        ensureBlock(role, title);
        auto blk = &convoBlocks[$-1];
        size_t idx = blk.lines.length;
        blk.lines ~= line;
        blk.payloads ~= (payloadPresent ? payload : JSONValue.init);
        blk.dirtyFrom = min(blk.dirtyFrom, idx);
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
                    if (blk.payloads.length == blk.lines.length + toDrop) {
                        convoBlocks[bi].payloads = convoBlocks[bi].payloads[toDrop .. $].dup;
                    }
                    convoBlocks[bi].dirtyFrom = 0;
                    convoBlocks[bi].cache.dataVer = uint.max; // force recompute
                    toDrop = 0;
                }
            }
        }
    }

    void appendToLastLine(string role, string fragment, JSONValue payload = JSONValue.init, bool payloadPresent = false) {
        ensureBlock(role);
        auto blk = &convoBlocks[$-1];
        if (blk.lines.length == 0) {
            blk.lines ~= fragment;
            blk.payloads ~= (payloadPresent ? payload : JSONValue.init);
        } else {
            size_t idx = blk.lines.length - 1;
            blk.lines[$-1] ~= fragment;
            if (payloadPresent && blk.payloads.length == blk.lines.length)
                blk.payloads[$-1] = payload;
            else if (payloadPresent && blk.payloads.length == blk.lines.length - 1)
                blk.payloads ~= payload;
            blk.dirtyFrom = min(blk.dirtyFrom, idx);
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

    void appendStream(string role, string text, JSONValue payload = JSONValue.init, string title = "", bool payloadPresent = false, bool forceNewLine = false) {
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
                    bool useNewLine = forceNewLine || startNewLine || start > 0 || pendingNewline;
                    if (useNewLine)
                        pushChatLine(role, dec, payload, title, payloadPresent);
                    else
                        appendToLastLine(role, dec, payload, payloadPresent);
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
                appendToLastLine(role, lineDec, payload, payloadPresent);
            } else {
                pushChatLine(role, lineDec, payload, title, payloadPresent);
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

    LogBlock* findToolBlock(string role) {
        // search from the end for the latest matching block
        for (long i = cast(long)convoBlocks.length - 1; i >= 0; --i) {
            if (convoBlocks[i].role == role) {
                return &convoBlocks[i];
            }
        }
        return null;
    }

    void handleToolUpdate(string role, string title, string text, JSONValue payload, bool isInitial, string status) {
        flushPending(); // avoid mixing with streaming buffers
        LogBlock* blk = findToolBlock(role);
        if (isInitial || blk is null) {
            LogBlock nb;
            nb.role = role;
            nb.title = title;
            if (text.length)
                nb.lines ~= text;
            nb.payloadMerged = payload;
            nb.hasPayloadMerged = (payload.type != JSONType.null_);
            nb.status = status;
            nb.ver = 1;
        convoBlocks ~= nb;
        convoContentAdded = true;
        return;
    }
        // update existing block
        if (text.length) {
            if (blk.lines.length == 0)
                blk.lines ~= text;
            else
                blk.lines[$-1] ~= text;
        }
        if (payload.type != JSONType.null_) {
            if (blk.hasPayloadMerged) blk.payloadMerged = mergeJson(blk.payloadMerged, payload);
            else blk.payloadMerged = payload;
            blk.hasPayloadMerged = true;
        }
        if (status.length) blk.status = status;
        blk.ver++;
        convoContentAdded = true;
    }

    void handlePlanUpdate(string text, JSONValue payload, string status) {
        flushPending();
        if (!hasPlan) {
            hasPlan = true;
            planView = PlanView.init;
        }
        if (text.length) {
            planView.lines = splitLines(text);
        }
        if (payload.type != JSONType.null_) {
            if (planView.payload.type != JSONType.null_)
                planView.payload = mergeJson(planView.payload, payload);
            else
                planView.payload = payload;
        }
        if (status.length) planView.status = status;
        planView.ver++;
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
            const bool isPermReq = (m == "session/request_permission"
                || m == "request_permission"
                || m == "request_permissions"
                || m == "request_prermissions"); // tolerate legacy/misspelled names
            if (isPermReq) {
                string pid;
                if ("id" in obj.object) pid = obj["id"].toString();
                if (pid.length == 0 && "params" in obj.object && obj["params"].type == JSONType.object) {
                    auto p = obj["params"];
                    if ("id" in p.object) pid = p["id"].toString();
                    else if ("requestId" in p.object) pid = p["requestId"].toString();
                    else if ("request_id" in p.object) pid = p["request_id"].toString();
                }
                string reason;
                if ("params" in obj.object && obj["params"].type == JSONType.object
                    && "reason" in obj["params"].object && obj["params"]["reason"].type == JSONType.string) {
                    reason = obj["params"]["reason"].str;
                }
                permQueue ~= PermissionRequest(pid, reason, obj["params"], false, false);
                permChanged = true;
                pushResult("log", false, obj.toString());
                return;
            }
            if (m == "session/update") {
                auto params = obj["params"];
                if ("update" in params.object) {
                    auto upd = params["update"];
                    handleSessionUpdate(upd);
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
                } else if (c.startsWith("perm:")) {
                    auto parts = c.split(':');
                    if (parts.length >= 3) {
                        string pid = parts[1];
                        bool allow = (parts[2] == "yes");
                        try {
                            localClient.sendPermissionResponse(pid, allow);
                            pushResult("log", false, "permission " ~ (allow ? "granted" : "denied") ~ " id=" ~ pid);
                        } catch (Exception e) {
                            pushResult("log", true, "permission response failed: "~e.msg);
                        }
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

        auto cmd = parseCommandLine(incSettingsGet!string("ACP.Command"));
        auto workingDir = incSettingsGet!string("ACP.Workingdir");
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
        clearState();
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
        lastStartAttempt = MonoTime.currTime;
    }

protected:
    override void onUpdate() {
        // ImGui context guard: avoid touching style arrays when context is not ready
        auto ctxImgui = igGetCurrentContext();
        if (ctxImgui is null || ctxImgui.Style.Colors.length == 0) {
            return;
        }
        auto style = igGetStyle();

        // controls (right aligned)
        auto statusLabel = _("Status: %s").format(statusText).toStringz();
        igTextUnformatted(statusLabel);
        igSameLine();

        ImVec2 avail = incAvailableSpace();
        auto newLabel = "\ue266".toStringz();
        float btnSize = 24;
        float totalW = style.ItemSpacing.x + btnSize;
        float offsetX = (avail.x > totalW) ? (avail.x - totalW) : 0;
        incDummy(ImVec2(offsetX, 1));
        igSameLine();
        if (incButtonColored(newLabel, ImVec2(btnSize, btnSize))) {
            stopAgent();
            statusText = "Initializing...";
            clearState();
            startAgent();
        }
        if (igIsItemHovered()) {
            igBeginTooltip();
            igTextUnformatted(_("New session").toStringz());
            igEndTooltip();
        }

        // auto-start once when possible (cooldown 1s between attempts)
        auto now = MonoTime.currTime;
        if (!workerRunning && !initInFlight) {
            auto delta = now - lastStartAttempt;
            if (delta > seconds(1)) {
                startAgent();
            }
        }

        igSeparator();

        // Pending permission requests
        if (permQueue.length) {
            auto rq = permQueue[0];
            string reason = rq.reason.length ? rq.reason : "(no reason)";
            string optSummary;
            if (rq.params.type == JSONType.object && "options" in rq.params.object) {
                auto opts = rq.params["options"];
                if (opts.type == JSONType.array) {
                    optSummary = opts.array.length.to!string ~ " option(s)";
                }
            }
            igText(_("Permission requested: %s").format(reason).toStringz());
            if (rq.id.length == 0) {
                igTextColored(ImVec4(1, 0.3f, 0.3f, 1), _("Cannot respond: missing request id").toStringz());
            }
            if (optSummary.length) {
                igText(_("Options: %s").format(optSummary).toStringz());
            }
            if (!rq.responded) {
                if (igButton(_("Allow").toStringz())) {
                    enqueueCommand("perm:" ~ rq.id ~ ":yes");
                    permQueue[0].responded = true;
                    permQueue[0].granted = true;
                    promptPending = true;
                }
            igSameLine();
                if (igButton(_("Deny").toStringz())) {
                    enqueueCommand("perm:" ~ rq.id ~ ":no");
                    permQueue[0].responded = true;
                    permQueue[0].granted = false;
                    promptPending = true;
                }
            } else {
                igTextDisabled((rq.granted ? _("Sent: Allow") : _("Sent: Deny")).toStringz());
                if (igButton(_("Dismiss").toStringz())) {
                    permQueue = permQueue[1 .. $];
                }
                igSameLine();
                if (igButton(_("Resend").toStringz())) {
                    enqueueCommand("perm:" ~ rq.id ~ ":" ~ (rq.granted ? "yes" : "no"));
                    promptPending = true;
                }
            }
            igSeparator();
        }

        // handle results from worker
        drainResults();

        ImVec2 availLog = incAvailableSpace();
        float inputAreaH = igGetFrameHeightWithSpacing() * 2   // status + separator padding
                         + igGetStyle().ItemSpacing.y * 3      // tab bar + spacing
                         + igGetFrameHeightWithSpacing();      // user message line height
        float planAreaH = 0;
        if (hasPlan && planView.lines.length) {
            float lineH = igGetTextLineHeightWithSpacing();
            planAreaH = lineH * planView.lines.length + igGetStyle().ItemSpacing.y * 2;
        }
        ImVec2 logSize = ImVec2(availLog.x, availLog.y - inputAreaH - planAreaH);
        if (logSize.y < 160) logSize.y = availLog.y * 0.65f;

        // Conversation / Log tabs
        if (igBeginTabBar("AgentTabs", ImGuiTabBarFlags.None)) {
            if (igBeginTabItem(_("Conversation").toStringz())) {
                flushPending(); // show latest partial lines
                if (igBeginChild("AgentConversation", logSize, true,
                        ImGuiWindowFlags.AlwaysVerticalScrollbar)) {
                    auto styleConv = igGetStyle();
                    foreach (i, ref blk; convoBlocks) {
                        bool isLast = (i == convoBlocks.length - 1);
                        final switch (roleKind(blk)) {
                            case RoleKind.You:
                                renderYou(blk, logSize.x, styleConv.Colors[0]);
                                break;
                            case RoleKind.Tool:
                                renderTool(blk, logSize.x, isLast);
                                break;
                            case RoleKind.AssistantThinking:
                                renderAssistantThinking(blk, logSize.x);
                                break;
                            case RoleKind.AssistantFinal:
                                renderAssistantFinal(blk, logSize.x, isLast);
                                break;
                            case RoleKind.Error:
                                renderError(blk, logSize.x, isLast);
                                break;
                            case RoleKind.Other:
                                renderOther(blk, logSize.x, isLast);
                                break;
                        }
                    }
                }
                igEndChild();
                convoContentAdded = false;
                igEndTabItem();
            }
            if (igBeginTabItem(_("Log").toStringz())) {
                auto linesLog = expandLines(logLines);
                auto tvLog = trimBlankEdges(linesLog);
                auto viewLog = linesLog[tvLog.start .. tvLog.end];
                if (igBeginChild("AgentLog", logSize, true,
                        ImGuiWindowFlags.AlwaysVerticalScrollbar)) {
                    if (tvLog.trimmed) {
                        LineHeightCache tmp;
                        renderClippedLines(viewLog, logSize.x, logVersion, tmp);
                    } else {
                        renderClippedLines(viewLog, logSize.x, logVersion, logCache);
                    }
                }
                igEndChild();
                logContentAdded = false;
                igEndTabItem();
            }
            igEndTabBar();
        }

        // Plan overlay (shown until a new plan arrives)
        if (hasPlan && planView.lines.length) {
            igSeparator();
            igText(_("Plan").toStringz());
            ImU32 colStatus = statusColor(planView.status.length ? planView.status : "in_progress");
            ImVec4 col;
            igColorConvertU32ToFloat4(&col, colStatus);
            foreach (idx, line; planView.lines) {
                auto label = format("%d. %s", idx + 1, line).toStringz();
                igTextColored(col, label);
            }
            igSeparator();
        }

        igSeparator();
        igText(_("User message:").toStringz());
        float sendBtnW = 80;
        float textW = availLog.x - sendBtnW - igGetStyle().ItemSpacing.x;
        if (textW < 160) textW = availLog.x * 0.75f;
        bool awaitingPermission = permQueue.length > 0;
        bool awaitingAgent = promptPending || awaitingPermission;
        bool submit = incInputText("##acp_user_text", textW, userText, ImGuiInputTextFlags.EnterReturnsTrue);
        if (submit && userText.length) {
            if (!awaitingAgent) enqueueUserText();
        }
        igSameLine();
        if (!awaitingAgent) {
            if (incButtonColored(_("\ue163").toStringz(), ImVec2(sendBtnW, 0))) {
                if (userText.length) {
                    enqueueUserText();
                }
            }
        } else {
            if (incButtonColored(_("\ue5c9").toStringz(), ImVec2(sendBtnW, 0))) {
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
