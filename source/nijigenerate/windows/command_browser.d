/*
    Command Browser window: list all commands, show inputs/outputs/descriptions.
*/
module nijigenerate.windows.command_browser;

import nijigenerate.windows.base;
import nijigenerate.widgets; // incBeginCategory helpers
import nijigenerate.widgets.controller : incController;
import nijigenerate.widgets.inputtext : incInputText;
import nijigenerate.commands; // AllCommandMaps
import nijigenerate.commands.base : BaseExArgsOf, TW, CreateResult, DeleteResult, LoadResult, ExCommandResult;
import nijigenerate.commands.viewport.palette : filterCommands; // shared filtering
import nijigenerate.core.shortcut.base : ngBuildExecutionContext;
import nijigenerate.project : incActivePuppet;
import nijilive.core.puppet : Puppet;
import nijilive.core.nodes : Node;
import nijilive.core.param : Parameter;
import nijilive.core.param.binding : ParameterBinding;
import inmath : vec2u;
import i18n;
import std.string : toLower, format, toStringz, fromStringz, strip;
import std.array : array, join;
import std.traits : TemplateArgsOf, isInstanceOf, ReturnType;
import std.traits : EnumMembers;
import std.meta : AliasSeq;
import std.conv : to;
import std.algorithm.searching : canFind;
import std.algorithm : sort, map;
import std.traits : isIntegral, isFloatingPoint;
import nijilive.core.param.binding : ParameterBinding;
import nijigenerate.utils : incTypeIdToIcon;
import bindbc.imgui : ImGuiStyleVar, igPushStyleVar, igPopStyleVar, igGetStyle, ImGuiSliderFlags;
import std.math : isNaN;

// Binding id map (session-scoped) — used only for stable ImGui IDs
__gshared uint[ParameterBinding] gBindingIds;
__gshared ParameterBinding[uint] gBindingById;
__gshared uint gBindingNextId = 1;

bool incIsCommandBrowserOpen;

private struct CommandArgInfo {
    string name;
    string typeName;
    string desc;
    bool hidden;
    string[] enumValues;
    bool isIntegral;
    bool isFloat;
    bool isBool;
}

private struct CommandInfo {
    string id;         // EnumType.Value
    string category;   // Enum type name
    Command cmd;
    CommandArgInfo[] inputs;
    string outputType;
    bool isResource;
    string resourceType;
    string resourceChange; // Created/Deleted/Loaded (if known)
    bool isExResult;
    string exResultType;
    void function(Command, string[string]) applyArgs; // optional arg applier (per concrete type)
    void function(Command, ref string[string]) captureDefaults; // optional default capturer
    string[] resourceArgs; // arg names that map to Node[]/Parameter[]/ParameterBinding[]
}

private __gshared CommandInfo[] gCommandInfos;
private __gshared CommandInfo[Command] gCommandInfosByCmd;

// Best-effort parser for common scalar types
private bool parseArgValue(T)(string raw, ref T outVal) {
    import std.conv : to;
    static if (is(T == string)) { outVal = raw; return true; }
    else static if (is(T == enum)) {
        try { outVal = to!T(raw); return true; } catch(Exception) { return false; }
    }
    else static if (is(T == bool)) {
        auto low = raw.toLower;
        if (low == "true" || low == "1" || low == "yes") { outVal = true; return true; }
        if (low == "false" || low == "0" || low == "no") { outVal = false; return true; }
        return false;
    } else static if (is(T == Parameter)) {
        import std.conv : to;
        try {
            auto id = to!uint(raw);
            auto p = incActivePuppet();
            if (p is null) return false;
            auto found = p.find!(Parameter)(id);
            if (found is null) return false;
            outVal = found;
            return true;
        } catch(Exception) { return false; }
    } else static if (is(T == Node)) {
        import std.conv : to;
        try {
            auto id = to!uint(raw);
            auto p = incActivePuppet();
            if (p is null) return false;
            auto found = p.find!(Node)(id);
            if (found is null) return false;
            outVal = found;
            return true;
        } catch(Exception) { return false; }
    } else static if (is(T == ParameterBinding)) {
        import std.conv : to;
        try {
            auto id = to!uint(raw);
            auto p = incActivePuppet();
            if (p is null) return false;
            foreach (param; p.parameters) {
                foreach (b; param.bindings) {
                    static uint ensureBindingId(ParameterBinding b) {
                        if (auto idp = b in gBindingIds) return *idp;
                        auto nid = gBindingNextId++;
                        gBindingIds[b] = nid;
                        gBindingById[nid] = b;
                        return nid;
                    }
                    if (ensureBindingId(b) == id) { outVal = b; return true; }
                }
            }
            return false;
        } catch(Exception) { return false; }
    } else static if (isIntegral!T) {
        try { outVal = to!T(raw); return true; } catch(Exception) { return false; }
    } else static if (isFloatingPoint!T) {
        try {
            auto v = to!T(raw);
            if (isNaN(v)) return false;
            outVal = v;
            return true;
        } catch(Exception) { return false; }
    } else {
        return false; // unsupported (e.g., Node/Parameter)
    }
}

// Extract key type of AA like V[K]
private template _KeyTypeOfAA(alias AA) {
    static if (is(typeof(AA) : V[K], V, K)) alias _KeyTypeOfAA = K;
    else alias _KeyTypeOfAA = void;
}
private template _ValueTypeOfAA(alias AA) {
    static if (is(typeof(AA) : V[K], V, K)) alias _ValueTypeOfAA = V;
    else alias _ValueTypeOfAA = void;
}

// Remove embedded NULs/control chars that break ImGui text input rendering
private string trimAtNul(string s) {
    import std.algorithm.searching : countUntil;
    auto idx = s.countUntil('\0');
    if (idx >= 0) return s[0 .. idx];
    return s;
}
private void textWrappedInColumn(const(char)* s) {
    if (s is null) { igText("-"); return; }
    auto wrapPos = igGetCursorPosX() + igGetColumnWidth();
    igPushTextWrapPos(wrapPos);
    igTextWrapped(s);
    igPopTextWrapPos();
}
private void textWrappedInColumn(string s) { textWrappedInColumn(toStringz(s)); }

private void rebuildCommandInfos() {
    gCommandInfos.length = 0;
    gCommandInfosByCmd.clear();
    static foreach (AA; AllCommandMaps) {{
        alias K = _KeyTypeOfAA!AA;
        alias V = _ValueTypeOfAA!AA;
        foreach (k, v; AA) {
            CommandArgInfo[] args;
            string outputType;
            bool isResource = false;
            string resourceType;
            string resourceChange;
            bool isExResult = false;
            string exResultType;
            void function(Command, string[string]) applier = null;
            void function(Command, ref string[string]) defCapturer = null;
            string[] resourceArgs;
            alias KT = typeof(k);
            static if (is(KT == enum)) {
            static foreach (m; EnumMembers!KT) {{
                if (k == m) {{
                    enum _mName  = __traits(identifier, m);
                    enum _typeName = _mName ~ "Command";
                    static if (__traits(compiles, mixin(_typeName))) {
                        alias C = mixin(_typeName);
                        static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                            applier = (Command c, string[string] vals) {
                                auto inst = cast(C)c;
                                if (inst is null) return;
                                alias Declared = BaseExArgsOf!C;
                                static foreach (i, Param; Declared) {{
                                    enum hidden = ({ bool h = false; foreach (attr; __traits(getAttributes, Param)) static if (is(attr == HideFromExpose)) h = true; return h; })();
                                    static if (isInstanceOf!(TW, Param)) {
                                        alias TParam = TemplateArgsOf!Param[0];
                                        enum fname = TemplateArgsOf!Param[1];
                                    } else {
                                        alias TParam = Param;
                                        enum fname = "arg" ~ i.to!string;
                                    }
                                    static if (!hidden && !is(TParam : Node) && !is(TParam : Parameter)) {
                                        if (auto pv = fname in vals) {
                                            TParam v;
                                            if (parseArgValue!TParam(*pv, v)) {
                                                mixin("inst."~fname~" = v;");
                                            }
                                        }
                                    }
                                }}
                            };
                        }
                        static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                            alias Declared = BaseExArgsOf!C;
                            static foreach (i, Param; Declared) {{
                                CommandArgInfo info;
                                enum hidden = ({ bool h = false; foreach (attr; __traits(getAttributes, Param)) static if (is(attr == HideFromExpose)) h = true; return h; })();
                                static if (isInstanceOf!(TW, Param)) {
                                    alias TParam = TemplateArgsOf!Param[0];
                                    enum fname = TemplateArgsOf!Param[1];
                                    enum fdesc = TemplateArgsOf!Param[2];
                                    enum isEnum = is(TParam == enum);
                                    info.name = fname;
                                    info.typeName = TParam.stringof;
                                    info.desc = enrichArgDesc!TParam(fdesc);
                                    info.isIntegral = isIntegral!TParam;
                                    info.isFloat = isFloatingPoint!TParam;
                                    info.isBool = is(TParam == bool);
                                    static if (isEnum) { foreach (o; EnumMembers!TParam) info.enumValues ~= o.stringof; }
                                    info.hidden = hidden;
                                    static if (is(TParam == Node[]) || is(TParam == Parameter[]) || is(TParam == ParameterBinding[])) {
                                        resourceArgs ~= fname;
                                    }
                                } else {
                                    info.name = "arg" ~ i.to!string;
                                    info.typeName = Param.stringof;
                                    info.desc = "";
                                    info.isIntegral = isIntegral!Param;
                                    info.isFloat = isFloatingPoint!Param;
                                    info.isBool = is(Param == bool);
                                    info.hidden = hidden;
                                }
                                if (!hidden) args ~= info;
                            }}
                        }
                            alias RunType = ReturnType!(typeof((cast(C)null).run));
                            outputType = RunType.stringof;
                            static if (is(RunType : CreateResult!RT, RT)) {
                                isResource = true;
                                resourceType = RT.stringof;
                                resourceChange = "Created";
                            } else static if (is(RunType : DeleteResult!RT, RT)) {
                                isResource = true;
                                resourceType = RT.stringof;
                                resourceChange = "Deleted";
                            } else static if (is(RunType : LoadResult!RT, RT)) {
                                isResource = true;
                                resourceType = RT.stringof;
                                resourceChange = "Loaded";
                            } else static if (is(RunType : ExCommandResult!RT, RT)) {
                                isExResult = true;
                                exResultType = RT.stringof;
                                static if (is(RT : CreateResult!RRT, RRT)) {
                                    isResource = true;
                                    resourceType = RRT.stringof;
                                    resourceChange = "Created";
                                } else static if (is(RT : DeleteResult!RRT, RRT)) {
                                    isResource = true;
                                    resourceType = RRT.stringof;
                                    resourceChange = "Deleted";
                                } else static if (is(RT : LoadResult!RRT, RRT)) {
                                    isResource = true;
                                    resourceType = RRT.stringof;
                                    resourceChange = "Loaded";
                                }
                            }

                            static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                                defCapturer = (Command c, ref string[string] vals) {
                                    auto inst = cast(C)c;
                                    if (inst is null) return;
                                    alias Declared = BaseExArgsOf!C;
                                static foreach (i, Param; Declared) {{
                                    static if (isInstanceOf!(TW, Param)) {
                                        alias TParam = TemplateArgsOf!Param[0];
                                        enum fname = TemplateArgsOf!Param[1];
                                        enum hidden = TemplateArgsOf!Param.length >= 4 ? TemplateArgsOf!Param[3] : false;
                                    } else {
                                        alias TParam = Param;
                                        enum fname = "arg" ~ i.to!string;
                                        enum hidden = false;
                                    }
                                    static if (!hidden && !is(TParam : Node[]) && !is(TParam : Parameter[]) && !is(TParam : ParameterBinding[])) {
                                        try {
                                            auto v = mixin("inst."~fname);
                                            // Truncate at first NUL for display, keep simple string conversion
                                            vals[fname] = trimAtNul(to!string(v));
                                        } catch (Exception) {}
                                        }
                                    }}
                                };
                            }
                        }
                    }}
                }}
            } else static if (!is(V == void)) {
                // Non-enum keys (dynamic commands) — inspect the value type directly
                alias C = V;
                static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                    applier = (Command c, string[string] vals) {
                        auto inst = cast(C)c;
                        if (inst is null) return;
                        alias Declared = BaseExArgsOf!C;
                        static foreach (i, Param; Declared) {{
                            static if (isInstanceOf!(TW, Param)) {
                                alias TParam = TemplateArgsOf!Param[0];
                                enum fname = TemplateArgsOf!Param[1];
                                enum hidden = TemplateArgsOf!Param.length >= 4 ? TemplateArgsOf!Param[3] : false;
                            } else {
                                alias TParam = Param;
                                enum fname = "arg" ~ i.to!string;
                                enum hidden = false;
                            }
                            static if (!hidden && !is(TParam : Node) && !is(TParam : Parameter)) {
                                if (auto pv = fname in vals) {
                                    TParam v;
                                    if (parseArgValue!TParam(*pv, v)) {
                                        mixin("inst."~fname~" = v;");
                                    }
                                }
                            }
                        }}
                    };
                    alias Declared = BaseExArgsOf!C;
                    static foreach (i, Param; Declared) {{
                        CommandArgInfo info;
                        static if (isInstanceOf!(TW, Param)) {
                            alias TParam = TemplateArgsOf!Param[0];
                            enum fname = TemplateArgsOf!Param[1];
                            enum fdesc = TemplateArgsOf!Param[2];
                            enum hidden = TemplateArgsOf!Param.length >= 4 ? TemplateArgsOf!Param[3] : false;
                            enum isEnum = is(TParam == enum);
                            info.name = fname;
                            info.typeName = TParam.stringof;
                            info.desc = enrichArgDesc!TParam(fdesc);
                            static if (isEnum) { foreach (o; EnumMembers!TParam) info.enumValues ~= o.stringof; }
                            info.hidden = hidden;
                            static if (is(TParam == Node[]) || is(TParam == Parameter[]) || is(TParam == ParameterBinding[])) {
                                resourceArgs ~= fname;
                            }
                        } else {
                        info.name = "arg" ~ i.to!string;
                        info.typeName = Param.stringof;
                        info.desc = "";
                        enum hidden = false;
                        info.hidden = hidden;
                    }
                    if (!hidden) args ~= info;
                }}
            }
            alias RunType = ReturnType!(typeof((cast(C)null).run));
            outputType = RunType.stringof;
            static if (is(RunType : CreateResult!RT, RT)) {
                    isResource = true;
                    resourceType = RT.stringof;
                    resourceChange = "Created";
                } else static if (is(RunType : DeleteResult!RT, RT)) {
                    isResource = true;
                    resourceType = RT.stringof;
                    resourceChange = "Deleted";
                } else static if (is(RunType : LoadResult!RT, RT)) {
                    isResource = true;
                    resourceType = RT.stringof;
                    resourceChange = "Loaded";
                } else static if (is(RunType : ExCommandResult!RT, RT)) {
                    isExResult = true;
                    exResultType = RT.stringof;
                    static if (is(RT : CreateResult!RRT, RRT)) {
                        isResource = true;
                        resourceType = RRT.stringof;
                        resourceChange = "Created";
                    } else static if (is(RT : DeleteResult!RRT, RRT)) {
                        isResource = true;
                        resourceType = RRT.stringof;
                        resourceChange = "Deleted";
                    } else static if (is(RT : LoadResult!RRT, RRT)) {
                        isResource = true;
                        resourceType = RRT.stringof;
                        resourceChange = "Loaded";
                    }
                }

                static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                    defCapturer = (Command c, ref string[string] vals) {
                        auto inst = cast(C)c;
                        if (inst is null) return;
                        alias Declared = BaseExArgsOf!C;
                        static foreach (i, Param; Declared) {{
                            static if (isInstanceOf!(TW, Param)) {
                                alias TParam = TemplateArgsOf!Param[0];
                                enum fname = TemplateArgsOf!Param[1];
                            } else {
                                alias TParam = Param;
                                enum fname = "arg" ~ i.to!string;
                            }
                            static if (!is(TParam : Node) && !is(TParam : Parameter) && !is(TParam : ParameterBinding)) {
                                try {
                                    auto v = mixin("inst."~fname);
                                    vals[fname] = trimAtNul(to!string(v));
                                } catch (Exception) {}
                            }
                        }}
                    };
                }
            }

            auto info = CommandInfo(
                K.stringof ~ "." ~ to!string(k),
                K.stringof,
                v,
                args,
                outputType,
                isResource,
                resourceType,
                resourceChange,
                isExResult,
                exResultType,
                applier,
                defCapturer,
                resourceArgs
            );
            gCommandInfos ~= info;
            gCommandInfosByCmd[v] = info;
        }
    }}
}

class CommandBrowserWindow : Window {
private:
    string filterText;
    size_t selectedIndex;
    Command selectedCmd;
    bool initialized;
    string[string] argValues; // per-arg raw input
    string ctxNodes, ctxParams, ctxArmed, ctxKeyPoint;
    bool lastHasResult;
    bool lastSucceeded;
    string lastMessage;
    string lastResultType;
    string lastResultDetail;
    string[] lastCreated;
    string[] lastDeleted;
    string[] lastLoaded;
    Node[][string] argNodeSelections;
    Parameter[][string] argParamSelections;
    ParameterBinding[][string] argBindingSelections;
    Node[] ctxNodesSel;
    Parameter[] ctxParamsSel;
    Parameter[] ctxArmedSel;
    ParameterBinding[] ctxBindingsSel;
    uint newPickerTemp;
    bool contextDirty;
    void resetSelection(CommandInfo info) {
        selectedCmd = info.cmd;
        argValues = null;
        lastHasResult = false;
        lastSucceeded = false;
        lastMessage = lastResultType = lastResultDetail = "";
        lastCreated = lastDeleted = lastLoaded = null;
        argNodeSelections = null;
        argParamSelections = null;
        argBindingSelections = null;
        ctxNodesSel = null;
        ctxParamsSel = null;
        ctxArmedSel = null;
        ctxBindingsSel = null;
        if (info.captureDefaults !is null) {
            info.captureDefaults(selectedCmd, argValues);
        }
        captureDefaultContext();
    }

    void captureDefaultContext() {
        auto ctx = ngBuildExecutionContext();
        ctxNodesSel = ctx.hasNodes ? ctx.nodes : [];
        ctxParamsSel = ctx.hasParameters ? ctx.parameters : [];
        ctxArmedSel = ctx.hasArmedParameters ? ctx.armedParameters : [];
        ctxBindingsSel = ctx.hasBindings ? ctx.bindings : [];
        if (ctx.hasKeyPoint) {
            ctxKeyPoint = format("%s,%s", ctx.keyPoint.x, ctx.keyPoint.y);
        } else {
            ctxKeyPoint = "";
        }
        ctxNodes = ctxParams = ctxArmed = "";
        contextDirty = false;
    }

    void drawInputsTable(ref CommandInfo info) {
        if (igBeginTable("inputs", 4, ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders | ImGuiTableFlags.Resizable)) {
            igTableSetupColumn(__("Name"));
            igTableSetupColumn(__("Type"));
            igTableSetupColumn(__("Description"));
            igTableSetupColumn(__("Value"));
            igTableHeadersRow();
            foreach (arg; info.inputs) {
                igPushID(toStringz(arg.name));
                igTableNextRow();
                igTableSetColumnIndex(0); igText(toStringz(arg.name));
                igTableSetColumnIndex(1); igText(toStringz(arg.typeName));
                igTableSetColumnIndex(2);
                textWrappedInColumn(arg.desc.length ? arg.desc : "-");
                igTableSetColumnIndex(3);
                if (arg.enumValues.length) {
                    if (argValues is null || arg.name !in argValues) {
                        argValues[arg.name] = arg.enumValues.length ? arg.enumValues[0] : "";
                    }
                    auto current = argValues[arg.name];
                    if (igBeginCombo(toStringz("##enum_"~arg.name), toStringz(current.length ? current : "-"))) {
                        foreach (opt; arg.enumValues) {
                            bool sel = (opt == current);
                            if (igSelectable(toStringz(opt), sel)) {
                                argValues[arg.name] = opt;
                                current = opt;
                            }
                        }
                        igEndCombo();
                    }
                } else if (arg.isBool) {
                    if (argValues is null || arg.name !in argValues) argValues[arg.name] = "false";
                    bool v = false;
                    parseArgValue!bool(argValues[arg.name], v);
                    string label = "##b_"~arg.name;
                    if (ngToggleSwitch(label, v, ImVec2(38, 20))) {
                        argValues[arg.name] = v ? "true" : "false";
                    }
                } else if (arg.isFloat) {
                    if (argValues is null || arg.name !in argValues) argValues[arg.name] = "";
                    float v = 0;
                    bool hasVal = argValues[arg.name].length && parseArgValue!float(argValues[arg.name], v);
                    if (!hasVal) { v = 0; argValues[arg.name] = "0"; } // keep internal arg to match displayed value
                    string label = "##f_"~arg.name;
                    // Align with inspector-style dragging input
                    if (incDragFloat(label, &v, 0.1f, -float.max, float.max, "%.4f", ImGuiSliderFlags.NoRoundToFormat)) {
                        argValues[arg.name] = to!string(v);
                    }
                } else if (arg.isIntegral) {
                    if (argValues is null || arg.name !in argValues) argValues[arg.name] = "";
                    int v = 0;
                    bool hasVal = argValues[arg.name].length && parseArgValue!int(argValues[arg.name], v);
                    if (!hasVal) { v = 0; argValues[arg.name] = "0"; }
                    auto label = toStringz("##i_"~arg.name);
                    if (igInputInt(label, &v)) {
                        argValues[arg.name] = to!string(v);
                    }
                } else if (arg.typeName == "Node[]") {
                    renderResourcePicker!Node(arg.name, argNodeSelections[arg.name], incActivePuppet());
                } else if (arg.typeName == "Parameter[]") {
                    renderResourcePicker!Parameter(arg.name, argParamSelections[arg.name], incActivePuppet());
                } else if (arg.typeName == "ParameterBinding[]") {
                    renderResourcePicker!ParameterBinding(arg.name, argBindingSelections[arg.name], incActivePuppet());
                } else if (arg.typeName == "Parameter") {
                    auto sel = argParamSelections.get(arg.name, cast(Parameter[])null);
                    if (sel is null) sel = [];
                    // limit to single selection
                    if (sel.length > 1) sel = sel[0 .. 1];
                    if (renderResourcePicker!Parameter(arg.name, sel, incActivePuppet())) {
                        if (sel.length > 1) sel = sel[0 .. 1];
                        argParamSelections[arg.name] = sel;
                    }
                    if (sel.length > 0 && sel[0] !is null) {
                        argValues[arg.name] = to!string(sel[0].uuid);
                    } else {
                        argValues[arg.name] = "";
                    }
                } else {
                    if (argValues is null || arg.name !in argValues) argValues[arg.name] = "";
                    auto key = format("arg_%s", arg.name);
                    incInputText(key, fromStringz(arg.name).idup, argValues[arg.name]);
                }
                igPopID();
                // Only mark context dirty for the context rows (handled below)
            }
            // Context override rows (inline with inputs)
            igTableNextRow(); igTableSetColumnIndex(0); igText("context.nodes"); igTableSetColumnIndex(1); igText("Node[]"); igTableSetColumnIndex(2); igText(__("Select nodes (optional)")); igTableSetColumnIndex(3);
            if (renderResourcePicker!Node("ctx_nodes", ctxNodesSel, incActivePuppet())) contextDirty = true;
            igTableNextRow(); igTableSetColumnIndex(0); igText("context.parameters"); igTableSetColumnIndex(1); igText("Parameter[]"); igTableSetColumnIndex(2); igText(__("Select parameters (optional)")); igTableSetColumnIndex(3);
            if (renderResourcePicker!Parameter("ctx_params", ctxParamsSel, incActivePuppet())) contextDirty = true;
            igTableNextRow(); igTableSetColumnIndex(0); igText("context.armedParameters"); igTableSetColumnIndex(1); igText("Parameter[]"); igTableSetColumnIndex(2); igText(__("Select armed parameters (optional)")); igTableSetColumnIndex(3);
            if (renderResourcePicker!Parameter("ctx_armed", ctxArmedSel, incActivePuppet())) contextDirty = true;
            igTableNextRow(); igTableSetColumnIndex(0); igText("context.bindings"); igTableSetColumnIndex(1); igText("ParameterBinding[]"); igTableSetColumnIndex(2); igText(__("Select bindings (optional)")); igTableSetColumnIndex(3);
            if (renderResourcePicker!ParameterBinding("ctx_bindings", ctxBindingsSel, incActivePuppet())) contextDirty = true;
            igTableNextRow(); igTableSetColumnIndex(0); igText("context.keyPoint"); igTableSetColumnIndex(1); igText("[uint,uint]"); igTableSetColumnIndex(2); igText(__("Key point (pick via controller), optional")); igTableSetColumnIndex(3);
            // Build parameter candidates
            Parameter[] kpParams = ctxParamsSel.length ? ctxParamsSel : ctxArmedSel.length ? ctxArmedSel : ctxParamsSel;
            if (!kpParams.length && argParamSelections.length) {
                foreach (_, selList; argParamSelections) {
                    if (selList.length) { kpParams = [selList[0]]; break; }
                }
            }
            // Current label
            auto noneLbl = to!string(__("- (none) -"));
            string currentLbl = ctxKeyPoint.length ? ctxKeyPoint : noneLbl;
            if (igButton(toStringz(currentLbl))) {
                igOpenPopup("ctx_kp_popup");
            }
            if (igBeginPopup("ctx_kp_popup")) {
                if (kpParams.length == 0 || kpParams[0] is null) {
                    igText(__("No Parameter selected"));
                } else {
                    static int kpParamIdx = 0;
                    if (kpParamIdx >= cast(int)kpParams.length) kpParamIdx = 0;
                    // Parameter selector if multiple
                    if (kpParams.length > 1) {
                        if (igBeginCombo("##kp_param_combo", toStringz(kpParams[kpParamIdx].name))) {
                            foreach (i, p; kpParams) {
                                if (p is null) continue;
                                bool sel = (i == kpParamIdx);
                                if (igSelectable(toStringz(p.name), sel))
                                    kpParamIdx = cast(int)i;
                            }
                            igEndCombo();
                        }
                    } else {
                        igText(toStringz(kpParams[kpParamIdx].name));
                    }
                    auto p = kpParams[kpParamIdx];
                    ImVec2 ctrlSize = p.isVec2 ? ImVec2(240, 160) : ImVec2(240, 64);
                    // Keep parameter value intact; use controller only for picking
                    auto prevVal = p.value;
                    if (incController("ctx_kp_ctrl", p, ctrlSize, true)) {
                        auto kp = p.findClosestKeypoint();
                        ctxKeyPoint = format("%s,%s", kp.x, kp.y);
                        contextDirty = true;
                    }
                    p.value = prevVal;
                    igSeparator();
                    igText(__("Click on controller to pick keypoint."));
                }
                igEndPopup();
            }
            igEndTable();
        }
    }

    void drawOutputsTable(ref CommandInfo info) {
        if (igBeginTable("outputs", 4, ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders | ImGuiTableFlags.Resizable)) {
            igTableSetupColumn(__("Field"));
            igTableSetupColumn(__("Type"));
            igTableSetupColumn(__("Meaning"));
            igTableSetupColumn(__("Value"));
            igTableHeadersRow();
            igTableNextRow(); igTableSetColumnIndex(0); igText("type"); igTableSetColumnIndex(1); igText(toStringz(info.outputType.length ? info.outputType : "CommandResult")); igTableSetColumnIndex(2); textWrappedInColumn(__("Declared return type")); igTableSetColumnIndex(3); igText(toStringz(lastHasResult ? (lastResultType.length ? lastResultType : info.outputType) : "-"));
            igTableNextRow(); igTableSetColumnIndex(0); igText("succeeded"); igTableSetColumnIndex(1); igText("bool"); igTableSetColumnIndex(2); textWrappedInColumn(__("True on success, false on failure.")); igTableSetColumnIndex(3); igText(toStringz(lastHasResult ? (lastSucceeded ? "true" : "false") : "-"));
            igTableNextRow(); igTableSetColumnIndex(0); igText("message"); igTableSetColumnIndex(1); igText("string"); igTableSetColumnIndex(2); textWrappedInColumn(__("Optional user-facing message.")); igTableSetColumnIndex(3); igText(toStringz(lastHasResult && lastMessage.length ? lastMessage : "-"));
            if (info.isResource) {
                igTableNextRow(); igTableSetColumnIndex(0); igText("resourceType"); igTableSetColumnIndex(1); igText("string"); igTableSetColumnIndex(2); textWrappedInColumn(__("Resource element type")); igTableSetColumnIndex(3); igText(toStringz(info.resourceType));
                if (info.resourceChange.length) {
                    igTableNextRow(); igTableSetColumnIndex(0); igText("change"); igTableSetColumnIndex(1); igText("string"); igTableSetColumnIndex(2); textWrappedInColumn(__("Created/Deleted/Loaded (hint)")); igTableSetColumnIndex(3); igText(toStringz(info.resourceChange));
                }
                const(char)[] rtype = info.resourceType.length ? info.resourceType : "Resource";
                bool showCreated = !info.resourceChange.length || info.resourceChange == "Created";
                bool showDeleted = !info.resourceChange.length || info.resourceChange == "Deleted";
                bool showLoaded  = !info.resourceChange.length || info.resourceChange == "Loaded";
                if (showCreated) {
                    igTableNextRow(); igTableSetColumnIndex(0); igText("created"); igTableSetColumnIndex(1); igText(toStringz(format("%s[]", rtype)));
                    igTableSetColumnIndex(2); textWrappedInColumn(__("Resources created (may be empty)"));
                    igTableSetColumnIndex(3);
                    if (lastHasResult && lastCreated.length) {
                        igText(toStringz(lastCreated.join(", ")));
                    } else {
                        igText("-");
                    }
                }
                if (showDeleted) {
                    igTableNextRow(); igTableSetColumnIndex(0); igText("deleted"); igTableSetColumnIndex(1); igText(toStringz(format("%s[]", rtype)));
                    igTableSetColumnIndex(2); textWrappedInColumn(__("Resources deleted (may be empty)"));
                    igTableSetColumnIndex(3);
                    if (lastHasResult && lastDeleted.length) {
                        igText(toStringz(lastDeleted.join(", ")));
                    } else {
                        igText("-");
                    }
                }
                if (showLoaded)  {
                    igTableNextRow(); igTableSetColumnIndex(0); igText("loaded");  igTableSetColumnIndex(1); igText(toStringz(format("%s[]", rtype)));
                    igTableSetColumnIndex(2); textWrappedInColumn(__("Resources loaded (may be empty)"));
                    igTableSetColumnIndex(3);
                    if (lastHasResult && lastLoaded.length) {
                        igText(toStringz(lastLoaded.join(", ")));
                    } else {
                        igText("-");
                    }
                }
            } else if (info.isExResult) {
                igTableNextRow(); igTableSetColumnIndex(0); igText("result"); igTableSetColumnIndex(1); igText(toStringz(info.exResultType.length ? info.exResultType : "T")); igTableSetColumnIndex(2); textWrappedInColumn(__("Wrapped ExCommand result type")); igTableSetColumnIndex(3); igText(toStringz(lastHasResult ? lastResultDetail : "-"));
            }
            igEndTable();
        }
    }

    void drawContextTable() {
        if (igBeginTable("context", 3, ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders | ImGuiTableFlags.Resizable)) {
            igTableSetupColumn(__("Context key"));
            igTableSetupColumn(__("Type"));
            igTableSetupColumn(__("Notes"));
            igTableHeadersRow();

            igTableNextRow(); igTableSetColumnIndex(0); igText("parameters");
            igTableSetColumnIndex(1); igText("uint[]");
            igTableSetColumnIndex(2); igText(__("Optional. Parameter UUIDs. Used by many commands."));

            igTableNextRow(); igTableSetColumnIndex(0); igText("armedParameters");
            igTableSetColumnIndex(1); igText("uint[]");
            igTableSetColumnIndex(2); igText(__("Optional. Parameters to arm before execution."));

            igTableNextRow(); igTableSetColumnIndex(0); igText("nodes");
            igTableSetColumnIndex(1); igText("uint[]");
            igTableSetColumnIndex(2); igText(__("Optional. Node UUIDs. Used by inspector/node commands."));

            igTableNextRow(); igTableSetColumnIndex(0); igText("keyPoint");
            igTableSetColumnIndex(1); igText("[uint,uint]");
            igTableSetColumnIndex(2); igText(__("Optional. Key point index (x,y)."));

            igTableNextRow(); igTableSetColumnIndex(0); igText("required?");
            igTableSetColumnIndex(1); igText(__("(per-command)"));
            igTableSetColumnIndex(2); igText(__("Requirements depend on each command's runnable/context check; not declared statically."));

            igEndTable();
        }
    }

public:
    this() {
        super(_("Command Browser"));
        initialized = false;
        argValues = null;
        ctxNodes = ctxParams = ctxArmed = ctxKeyPoint = "";
        lastHasResult = false;
        lastSucceeded = false;
        lastMessage = lastResultType = lastResultDetail = "";
        lastCreated = lastDeleted = lastLoaded = null;
        argNodeSelections = null;
        argParamSelections = null;
        argBindingSelections = null;
        ctxNodesSel = null;
        ctxParamsSel = null;
        ctxArmedSel = null;
        ctxBindingsSel = null;
        contextDirty = false;
        captureDefaultContext();
    }

protected:
    override void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoSavedSettings;
        igSetNextWindowSize(ImVec2(900, 640), ImGuiCond.FirstUseEver);
        igSetNextWindowSizeConstraints(ImVec2(700, 480), ImVec2(float.max, float.max));
        incIsCommandBrowserOpen = true;
        // Always rebuild to reflect latest command signatures/types
        rebuildCommandInfos();
        if (!initialized) {
            filterText = "";
            selectedIndex = 0;
            selectedCmd = null;
            initialized = true;
            argValues = null;
            ctxNodes = ctxParams = ctxArmed = ctxKeyPoint = "";
            lastHasResult = false;
            lastSucceeded = false;
            lastMessage = lastResultType = lastResultDetail = "";
            lastCreated = lastDeleted = lastLoaded = null;
            argNodeSelections = null;
            argParamSelections = null;
            argBindingSelections = null;
            ctxNodesSel = null;
            ctxParamsSel = null;
            ctxArmedSel = null;
            ctxBindingsSel = null;
            // Capture default shortcut context on first open
            captureDefaultContext();
        }
        super.onBeginUpdate();
    }

    override void onEndUpdate() {
        incIsCommandBrowserOpen = false;
        super.onEndUpdate();
    }

    override void onUpdate() {
        // If user hasn't edited context, keep it synced with the latest shortcut context
        if (!contextDirty) {
            captureDefaultContext();
        }
        auto avail = incAvailableSpace();
        igPushItemWidth(-1);
        auto searchLabel = fromStringz(__("Search")).idup;
        incInputText("COMMAND_BROWSER_SEARCH", searchLabel, filterText);
        igPopItemWidth();
        igSameLine();
        if (igButton(__("Refresh"))) {
            rebuildCommandInfos();
        }

        // Filter commands
        Command[] filteredCmds = filterCommands(filterText);
        CommandInfo[] filtered;
        foreach (c; filteredCmds) {
            auto p = c in gCommandInfosByCmd;
            if (p) filtered ~= *p;
        }
        // Stabilize ordering to avoid flicker when AA order changes
        filtered.sort!((a, b) => a.cmd.label().toLower < b.cmd.label().toLower || (a.cmd.label().toLower == b.cmd.label().toLower && a.id < b.id));

        // Re-anchor selection every frame based on selectedCmd when available
        if (selectedCmd !is null) {
            foreach (i, info; filtered) {
                if (info.cmd is selectedCmd) { selectedIndex = i; break; }
            }
        }
        // Clamp if the current index is out of range
        if (selectedIndex >= filtered.length) selectedIndex = filtered.length ? filtered.length - 1 : 0;
        // If nothing is selected but we have results, default to current index
        if (filtered.length) {
            if (selectedCmd is null || filtered[selectedIndex].cmd !is selectedCmd) {
                resetSelection(filtered[selectedIndex]);
            }
        } else {
            selectedCmd = null;
        }

        float listWidth = avail.x * 0.35f;
        if (igBeginChild("command_list", ImVec2(listWidth, 0), true)) {
            foreach (i, info; filtered) {
                bool selected = (info.cmd is selectedCmd) || (selectedCmd is null && i == selectedIndex);
                if (igSelectable(toStringz(format("%s##cmd_%s", info.cmd.label(), info.id)), selected)) {
                    if (selectedCmd !is info.cmd) resetSelection(info);
                    selectedIndex = i;
                }
                if (igIsItemHovered()) incTooltip(info.id);
                igTextDisabled(toStringz(info.id));
            }
        }
        igEndChild();

        igSameLine();
        if (igBeginChild("command_detail", ImVec2(0, 0), true)) {
            if (filtered.length == 0) {
                igText(__("No commands match the filter."));
            } else {
                auto info = filtered[selectedIndex];
                igText(toStringz(format("%s", info.cmd.label())));
                igTextDisabled(toStringz(info.id));
                if (info.cmd.description().length) {
                    igTextWrapped(toStringz(info.cmd.description()));
                }
                igSeparator();
                igText(__("Inputs (arguments + optional context)"));
                drawInputsTable(info);
                igSeparator();
                igText(__("Run"));
                if (incButtonColored(__("Run Command"))) {
                    auto ctx = ngBuildExecutionContext();
                    auto puppet = incActivePuppet();
                    auto parseList = (ref string src, ref Parameter[] outParams, ref Parameter[] outArmed, ref Node[] outNodes) {
                        import std.algorithm : splitter;
                        foreach (part; src.splitter([','])) {
                            auto s = part.strip;
                            if (!s.length) continue;
                            try {
                                uint id = to!uint(s);
                                if (puppet !is null) {
                                    auto p = puppet.find!(Parameter)(id);
                                    if (p !is null) outParams ~= p;
                                    auto a = puppet.find!(Node)(id);
                                    if (a !is null) outNodes ~= a;
                                }
                            } catch (Exception) {}
                        }
                    };
                    Parameter[] paramsOverride, armedOverride;
                    Node[] nodesOverride;
                    // Use pickers if provided, otherwise parse text
                    if (ctxParamsSel.length) {
                        paramsOverride = ctxParamsSel;
                    } else {
                        parseList(ctxParams, paramsOverride, armedOverride, nodesOverride);
                    }
                    if (ctxArmedSel.length) {
                        armedOverride = ctxArmedSel;
                    } else if (ctxArmed.length) {
                        Parameter[] tmp;
                        Parameter[] armedTmp;
                        Node[] dummy;
                        parseList(ctxArmed, tmp, armedTmp, dummy);
                        if (armedTmp.length) armedOverride = armedTmp;
                    }
                    if (ctxNodesSel.length) {
                        nodesOverride = ctxNodesSel;
                    } else if (ctxNodes.length) {
                        Parameter[] dummy;
                        Node[] nodesTmp;
                        Parameter[] tmpParams;
                        parseList(ctxNodes, tmpParams, armedOverride, nodesOverride);
                        if (nodesOverride.length) nodesOverride = nodesOverride;
                    }
                    ParameterBinding[] bindingsOverride;
                    if (ctxBindingsSel.length) {
                        bindingsOverride = ctxBindingsSel;
                    }
                    if (paramsOverride.length) ctx.parameters = paramsOverride;
                    if (armedOverride.length) ctx.armedParameters = armedOverride;
                    if (nodesOverride.length) ctx.nodes = nodesOverride;
                    if (bindingsOverride.length) ctx.bindings = bindingsOverride;
                    if (ctxKeyPoint.length) {
                        import std.algorithm : splitter;
                        auto parts = ctxKeyPoint.splitter([',']).array;
                        if (parts.length >= 2) {
                            try {
                                uint x = to!uint(parts[0].strip);
                                uint y = to!uint(parts[1].strip);
                                ctx.keyPoint = vec2u(x, y);
                            } catch (Exception) {}
                        }
                    }

                    // Apply args if possible
                    if (info.applyArgs !is null && selectedCmd !is null) {
                        info.applyArgs(selectedCmd, argValues);
                    }
                    CommandResult res;
                    try {
                        res = selectedCmd.run(ctx);
                        lastHasResult = true;
                        lastSucceeded = res.succeeded;
                        lastMessage = res.message;
                        lastResultType = typeid(res).toString();
                        lastResultDetail = "";
                        lastCreated = lastDeleted = lastLoaded = [];
                        string labelNode(Node n) { return n is null ? "(null)" : n.name; }
                        string labelParam(Parameter p) { return p is null ? "(null)" : p.name; }
                        string labelBind(ParameterBinding b) { return b is null ? "(null)" : b.getName(); }
                        alias ResourceTypes = AliasSeq!(Node, Parameter, ParameterBinding);
                        static foreach (R; ResourceTypes) {{
                            if (auto cr = cast(CreateResult!R) res) {
                                foreach (r; cr.created) {
                                    static if (is(R == Node)) lastCreated ~= labelNode(r);
                                    else static if (is(R == Parameter)) lastCreated ~= labelParam(r);
                                    else static if (is(R == ParameterBinding)) lastCreated ~= labelBind(r);
                                }
                            }
                            if (auto dr = cast(DeleteResult!R) res) {
                                foreach (r; dr.deleted) {
                                    static if (is(R == Node)) lastDeleted ~= labelNode(r);
                                    else static if (is(R == Parameter)) lastDeleted ~= labelParam(r);
                                    else static if (is(R == ParameterBinding)) lastDeleted ~= labelBind(r);
                                }
                            }
                            if (auto lr = cast(LoadResult!R) res) {
                                foreach (r; lr.loaded) {
                                    static if (is(R == Node)) lastLoaded ~= labelNode(r);
                                    else static if (is(R == Parameter)) lastLoaded ~= labelParam(r);
                                    else static if (is(R == ParameterBinding)) lastLoaded ~= labelBind(r);
                                }
                            }
                            // ExCommandResult!T where T is e.g., CreateResult!R
                            if (auto ex = cast(ExCommandResult!R) res) {
                                static if (is(R : CreateResult!CR, CR)) {
                                    foreach (r; (cast(CreateResult!CR)ex).created) {
                                        static if (is(CR == Node)) lastCreated ~= labelNode(r);
                                        else static if (is(CR == Parameter)) lastCreated ~= labelParam(r);
                                        else static if (is(CR == ParameterBinding)) lastCreated ~= labelBind(r);
                                    }
                                } else static if (is(R : DeleteResult!CR, CR)) {
                                    foreach (r; (cast(DeleteResult!CR)ex).deleted) {
                                        static if (is(CR == Node)) lastDeleted ~= labelNode(r);
                                        else static if (is(CR == Parameter)) lastDeleted ~= labelParam(r);
                                        else static if (is(CR == ParameterBinding)) lastDeleted ~= labelBind(r);
                                    }
                                } else static if (is(R : LoadResult!CR, CR)) {
                                    foreach (r; (cast(LoadResult!CR)ex).loaded) {
                                        static if (is(CR == Node)) lastLoaded ~= labelNode(r);
                                        else static if (is(CR == Parameter)) lastLoaded ~= labelParam(r);
                                        else static if (is(CR == ParameterBinding)) lastLoaded ~= labelBind(r);
                                    }
                                }
                            }
                        }}
                        static if (is(typeof(res) : ExCommandResult!T, T)) {
                            lastResultDetail = T.stringof;
                        }
                    } catch (Exception e) {
                        lastHasResult = true;
                        lastSucceeded = false;
                        lastMessage = "Exception: " ~ e.msg;
                        lastResultType = "Exception";
                        lastResultDetail = "";
                    }
                }
                igSeparator();
                igText(__("Output (last run)"));
                drawOutputsTable(info);
            }
        }
        igEndChild();
    }
}
uint ensureBindingId(ParameterBinding b) {
    if (auto idp = b in gBindingIds) return *idp;
    auto id = gBindingNextId++;
    gBindingIds[b] = id;
    gBindingById[id] = b;
    return id;
}
void renderNodeTree(Node n, ref Node chosen) {
    if (n is null) return;
    ImGuiTreeNodeFlags flags = n.children.length == 0 ? cast(ImGuiTreeNodeFlags)(ImGuiTreeNodeFlags.Leaf | ImGuiTreeNodeFlags.NoTreePushOnOpen) : ImGuiTreeNodeFlags.None;
    flags |= ImGuiTreeNodeFlags.DefaultOpen;
    string icon = incTypeIdToIcon(n.typeId);
    if (igTreeNodeEx(toStringz(format("%s %s##node_%s", icon, n.name, n.uuid)), flags)) {
        if (igIsItemClicked()) chosen = n;
        if ((flags & ImGuiTreeNodeFlags.NoTreePushOnOpen) == 0) {
            foreach (c; n.children) renderNodeTree(c, chosen);
            igTreePop();
        }
    }
}
void renderParameterTree(Puppet p, ref Parameter chosen) {
    if (p is null) return;
    foreach (param; p.parameters) {
        ImGuiTreeNodeFlags flags = cast(ImGuiTreeNodeFlags)(param.bindings.length > 0 ? ImGuiTreeNodeFlags.None : (ImGuiTreeNodeFlags.Leaf | ImGuiTreeNodeFlags.NoTreePushOnOpen));
        flags |= ImGuiTreeNodeFlags.DefaultOpen;
        string pIcon = incTypeIdToIcon("Parameter");
        if (igTreeNodeEx(toStringz(format("%s %s##param_%s", pIcon, param.name, param.uuid)), flags)) {
            if (igIsItemClicked()) chosen = param;
            if ((flags & ImGuiTreeNodeFlags.NoTreePushOnOpen) == 0) {
                igTreePop();
            }
        }
    }
}
void renderBindingTree(Puppet p, ref ParameterBinding chosen) {
    if (p is null) return;
    foreach (param; p.parameters) {
        if (param.bindings.length == 0) continue;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags.DefaultOpen;
        string pIcon = incTypeIdToIcon("Parameter");
        if (igTreeNodeEx(toStringz(format("%s %s##param_binding_%s", pIcon, param.name, param.uuid)), flags)) {
            foreach (b; param.bindings) {
                uint id = ensureBindingId(b);
                string bIcon = incTypeIdToIcon("Binding");
                if (igTreeNodeEx(toStringz(format("%s %s##binding_%s", bIcon, b.getName(), id)), cast(ImGuiTreeNodeFlags)(ImGuiTreeNodeFlags.Leaf | ImGuiTreeNodeFlags.NoTreePushOnOpen))) {
                    if (igIsItemClicked()) chosen = b;
                }
            }
            igTreePop();
        }
    }
}
string labelFor(Node n) { return n is null ? to!string(__("- (none) -")) : n.name; }
string labelFor(Parameter p) { return p is null ? to!string(__("- (none) -")) : p.name; }
string labelFor(ParameterBinding b) { return b is null ? to!string(__("- (none) -")) : b.getName(); }

bool renderResourcePicker(T)(string id, ref T[] selected, Puppet puppet) {
    bool changed = false;
    if (selected is null) selected = [];
    igPushID(toStringz(id));
    // If empty, show only add button (axes-like behavior, same style as axes)
    if (selected.length == 0) {
        if (incButtonColored("", ImVec2(24, 24))) {
            selected ~= null; // placeholder; user picks actual resource
            changed = true;
        }
        igPopID();
        return changed;
    }

    // Existing rows with remove button
    size_t i = 0;
    while (i < selected.length) {
        auto sel = selected[i];
        igPushID(cast(int)i);
        string lbl = labelFor(sel);
        if (igBeginCombo("##combo", toStringz(lbl))) {
            auto style = igGetStyle();
            igPushStyleVar(ImGuiStyleVar.IndentSpacing, style.IndentSpacing * 0.25f);
            T chosen = sel;
            static if (is(T == Node)) {
                if (puppet !is null && puppet.root !is null) renderNodeTree(puppet.root, chosen);
            } else static if (is(T == Parameter)) {
                renderParameterTree(puppet, chosen);
            } else static if (is(T == ParameterBinding)) {
                renderBindingTree(puppet, chosen);
            }
            igEndCombo();
            igPopStyleVar(1);
            if (chosen !is sel) { selected[i] = chosen; changed = true; }
        }
        igSameLine(0, 0);
        if (incButtonColored("", ImVec2(24, 24))) {
            selected = selected[0 .. i] ~ selected[i+1 .. $];
            changed = true;
            igPopID();
            continue; // do not advance i; list is shifted
        }
        igPopID();
        ++i;
    }

    // Add button after rows (same line as last row)
    igSameLine(0, 0);
    if (incButtonColored("", ImVec2(24, 24))) {
        selected ~= null;
        changed = true;
    }
    igPopID();
    return changed;
}
