/*
    Command Browser window: list all commands, show inputs/outputs/descriptions.
*/
module nijigenerate.windows.command_browser;

import nijigenerate.windows.base;
import nijigenerate.widgets; // incBeginCategory helpers
import nijigenerate.widgets.inputtext : incInputText;
import nijigenerate.commands; // AllCommandMaps
import nijigenerate.commands.base : BaseExArgsOf, TW, CreateResult, DeleteResult, LoadResult, ExCommandResult;
import nijigenerate.commands.viewport.palette : filterCommands; // shared filtering
import i18n;
import std.string : toLower, format, toStringz, fromStringz;
import std.array : array, join;
import std.traits : TemplateArgsOf, isInstanceOf, ReturnType;
import std.traits : EnumMembers;
import std.meta : AliasSeq;
import std.conv : to;
import std.algorithm.searching : canFind;
import std.algorithm : sort;

bool incIsCommandBrowserOpen;

private struct CommandArgInfo {
    string name;
    string typeName;
    string desc;
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
}

private __gshared CommandInfo[] gCommandInfos;
private __gshared CommandInfo[Command] gCommandInfosByCmd;

// Extract key type of AA like V[K]
private template _KeyTypeOfAA(alias AA) {
    static if (is(typeof(AA) : V[K], V, K)) alias _KeyTypeOfAA = K;
    else alias _KeyTypeOfAA = void;
}
private template _ValueTypeOfAA(alias AA) {
    static if (is(typeof(AA) : V[K], V, K)) alias _ValueTypeOfAA = V;
    else alias _ValueTypeOfAA = void;
}

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
            alias KT = typeof(k);
            static if (is(KT == enum)) {
                static foreach (m; EnumMembers!KT) {{
                    if (k == m) {{
                        enum _mName  = __traits(identifier, m);
                        enum _typeName = _mName ~ "Command";
                        static if (__traits(compiles, mixin(_typeName))) {
                            alias C = mixin(_typeName);
                            static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                                alias Declared = BaseExArgsOf!C;
                                static foreach (i, Param; Declared) {{
                                    CommandArgInfo info;
                                    static if (isInstanceOf!(TW, Param)) {
                                        alias TParam = TemplateArgsOf!Param[0];
                                        enum fname = TemplateArgsOf!Param[1];
                                        enum fdesc = TemplateArgsOf!Param[2];
                                        info.name = fname;
                                        info.typeName = TParam.stringof;
                                        info.desc = fdesc;
                                    } else {
                                        info.name = "arg" ~ i.to!string;
                                        info.typeName = Param.stringof;
                                        info.desc = "";
                                    }
                                    args ~= info;
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
                        }
                    }}
                }}
            } else static if (!is(V == void)) {
                // Non-enum keys (dynamic commands) — inspect the value type directly
                alias C = V;
                static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                    alias Declared = BaseExArgsOf!C;
                    static foreach (i, Param; Declared) {{
                        CommandArgInfo info;
                        static if (isInstanceOf!(TW, Param)) {
                            alias TParam = TemplateArgsOf!Param[0];
                            enum fname = TemplateArgsOf!Param[1];
                            enum fdesc = TemplateArgsOf!Param[2];
                            info.name = fname;
                            info.typeName = TParam.stringof;
                            info.desc = fdesc;
                        } else {
                            info.name = "arg" ~ i.to!string;
                            info.typeName = Param.stringof;
                            info.desc = "";
                        }
                        args ~= info;
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
                exResultType
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

    void drawInputsTable(ref CommandInfo info) {
        if (igBeginTable("inputs", 3, ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders | ImGuiTableFlags.Resizable)) {
            igTableSetupColumn(__("Name"));
            igTableSetupColumn(__("Type"));
            igTableSetupColumn(__("Description"));
            igTableHeadersRow();
            foreach (arg; info.inputs) {
                igTableNextRow();
                igTableSetColumnIndex(0); igText(toStringz(arg.name));
                igTableSetColumnIndex(1); igText(toStringz(arg.typeName));
                igTableSetColumnIndex(2); igText(arg.desc.length ? toStringz(arg.desc) : "-");
            }
            igEndTable();
        }
    }

    void drawOutputsTable(ref CommandInfo info) {
        if (igBeginTable("outputs", 2, ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders | ImGuiTableFlags.Resizable)) {
            igTableSetupColumn(__("Field"));
            igTableSetupColumn(__("Meaning"));
            igTableHeadersRow();
            igTableNextRow(); igTableSetColumnIndex(0); igText("type"); igTableSetColumnIndex(1); igText(toStringz(info.outputType.length ? info.outputType : "CommandResult"));
            igTableNextRow(); igTableSetColumnIndex(0); igText("succeeded"); igTableSetColumnIndex(1); igText(__("True on success, false on failure."));
            igTableNextRow(); igTableSetColumnIndex(0); igText("message"); igTableSetColumnIndex(1); igText(__("Optional user-facing message."));
            if (info.isResource) {
                igTableNextRow(); igTableSetColumnIndex(0); igText("resourceType"); igTableSetColumnIndex(1); igText(toStringz(info.resourceType));
                if (info.resourceChange.length) {
                    igTableNextRow(); igTableSetColumnIndex(0); igText("change"); igTableSetColumnIndex(1); igText(toStringz(info.resourceChange));
                }
                const(char)[] rtype = info.resourceType.length ? info.resourceType : "Resource";
                bool showCreated = !info.resourceChange.length || info.resourceChange == "Created";
                bool showDeleted = !info.resourceChange.length || info.resourceChange == "Deleted";
                bool showLoaded  = !info.resourceChange.length || info.resourceChange == "Loaded";
                if (showCreated) { igTableNextRow(); igTableSetColumnIndex(0); igText("created"); igTableSetColumnIndex(1); igText(toStringz(format("%s[] (may be empty)", rtype))); }
                if (showDeleted) { igTableNextRow(); igTableSetColumnIndex(0); igText("deleted"); igTableSetColumnIndex(1); igText(toStringz(format("%s[] (may be empty)", rtype))); }
                if (showLoaded)  { igTableNextRow(); igTableSetColumnIndex(0); igText("loaded");  igTableSetColumnIndex(1); igText(toStringz(format("%s[] (may be empty)", rtype))); }
            } else if (info.isExResult) {
                igTableNextRow(); igTableSetColumnIndex(0); igText("result"); igTableSetColumnIndex(1); igText(toStringz(info.exResultType.length ? info.exResultType : "T"));
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
        }
        super.onBeginUpdate();
    }

    override void onEndUpdate() {
        incIsCommandBrowserOpen = false;
        super.onEndUpdate();
    }

    override void onUpdate() {
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
                selectedCmd = filtered[selectedIndex].cmd;
            }
        } else {
            selectedCmd = null;
        }

        float listWidth = avail.x * 0.35f;
        if (igBeginChild("command_list", ImVec2(listWidth, 0), true)) {
            foreach (i, info; filtered) {
                bool selected = (info.cmd is selectedCmd) || (selectedCmd is null && i == selectedIndex);
                if (igSelectable(toStringz(format("%s##cmd_%s", info.cmd.label(), info.id)), selected)) {
                    selectedIndex = i;
                    selectedCmd = info.cmd;
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
                igText(__("Context (shared schema)"));
                drawContextTable();
                igSeparator();
                igText(__("Inputs"));
                drawInputsTable(info);
                igSeparator();
                igText(__("Output"));
                drawOutputsTable(info);
            }
        }
        igEndChild();
    }
}
