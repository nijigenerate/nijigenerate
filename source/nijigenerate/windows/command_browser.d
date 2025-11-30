/*
    Command Browser window: list all commands, show inputs/outputs/descriptions.
*/
module nijigenerate.windows.command_browser;

import nijigenerate.windows.base;
import nijigenerate.widgets; // incBeginCategory helpers
import nijigenerate.commands; // AllCommandMaps
import nijigenerate.commands.base : BaseExArgsOf, TW;
import i18n;
import std.string : toLower, format, toStringz, fromStringz;
import std.array : array, join;
import std.traits : TemplateArgsOf, isInstanceOf;
import std.meta : AliasSeq;
import std.conv : to;
import std.algorithm.searching : countUntil, canFind;
import std.algorithm : filter;

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
}

private __gshared CommandInfo[] gCommandInfos;

// Extract key type of AA like V[K]
private template _KeyTypeOfAA(alias AA) {
    static if (is(typeof(AA) : V[K], V, K)) alias _KeyTypeOfAA = K;
    else alias _KeyTypeOfAA = void;
}

private void rebuildCommandInfos() {
    gCommandInfos.length = 0;
    static foreach (AA; AllCommandMaps) {{
        alias K = _KeyTypeOfAA!AA;
        foreach (k, v; AA) {
            CommandArgInfo[] args;
            alias C = typeof(v);
            static if (__traits(compiles, BaseExArgsOf!C) && !is(BaseExArgsOf!C == void)) {
                alias Declared = BaseExArgsOf!C;
                static foreach (i, Param; Declared) {
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
                }
            }

            gCommandInfos ~= CommandInfo(
                K.stringof ~ "." ~ to!string(k),
                K.stringof,
                v,
                args
            );
        }
    }}
}

class CommandBrowserWindow : Window {
private:
    char[256] filterBuf;
    string filterText;
    size_t selectedIndex;

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

    void drawOutputsTable() {
        if (igBeginTable("outputs", 2, ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders | ImGuiTableFlags.Resizable)) {
            igTableSetupColumn(__("Field"));
            igTableSetupColumn(__("Meaning"));
            igTableHeadersRow();
            igTableNextRow(); igTableSetColumnIndex(0); igText("succeeded"); igTableSetColumnIndex(1); igText(__("True on success, false on failure."));
            igTableNextRow(); igTableSetColumnIndex(0); igText("message"); igTableSetColumnIndex(1); igText(__("Optional user-facing message."));
            igTableNextRow(); igTableSetColumnIndex(0); igText("payload"); igTableSetColumnIndex(1); igText(__("Optional typed payload (e.g., ResourceResult)."));
            igEndTable();
        }
    }

public:
    this() {
        super(_("Command Browser"));
    }

protected:
    override void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoSavedSettings;
        incIsCommandBrowserOpen = true;
        if (gCommandInfos.length == 0) rebuildCommandInfos();
        super.onBeginUpdate();
    }

    override void onEndUpdate() {
        incIsCommandBrowserOpen = false;
        super.onEndUpdate();
    }

    override void onUpdate() {
        auto avail = incAvailableSpace();
        igPushItemWidth(-1);
        if (igInputText(__("Search"), filterBuf.ptr, filterBuf.length)) {
            filterText = cast(string) fromStringz(cast(const char*)filterBuf.ptr);
        }
        igPopItemWidth();
        igSameLine();
        if (igButton(__("Refresh"))) {
            rebuildCommandInfos();
        }

        // Filter commands
        string f = filterText.toLower();
        auto filtered = gCommandInfos.filter!(c => f.length == 0 ||
            c.id.toLower().canFind(f) ||
            c.cmd.label().toLower().canFind(f) ||
            c.cmd.description().toLower().canFind(f)).array;

        if (selectedIndex >= filtered.length) selectedIndex = filtered.length ? filtered.length - 1 : 0;

        float listWidth = avail.x * 0.35f;
        if (igBeginChild("command_list", ImVec2(listWidth, 0), true)) {
            foreach (i, info; filtered) {
                bool selected = (i == selectedIndex);
                if (igSelectable(toStringz(format("%s##cmd_%s", info.cmd.label(), info.id)), selected)) {
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
                igText(__("Inputs"));
                drawInputsTable(info);
                igSeparator();
                igText(__("Output"));
                drawOutputsTable();
            }
        }
        igEndChild();
    }
}
