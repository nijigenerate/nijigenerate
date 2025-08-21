module nijigenerate.commands.viewport.palette;

import bindbc.imgui;
import nijigenerate.commands.base;
import nijigenerate.widgets.inputtext; // incInputText
import nijigenerate.widgets.notification; // NotificationPopup
import nijigenerate.commands; // AllCommandMaps
import nijigenerate.widgets.dummy; // incAvailableSpace
import i18n;
import std.string : toStringz, toLower;
import std.algorithm.searching : canFind, countUntil;
import std.algorithm.comparison : min;

// Command Palette for viewport: searchable list of commands

enum PaletteCommand {
    // Note: keeping the enum member name as requested
    // This implies the command class will be ListCommandCommand.
    ListCommand,
}

Command[PaletteCommand] commands;

void ngInitCommands(T)() if (is(T == PaletteCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!PaletteCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}

// Internal state for the popup interaction
private __gshared string gPaletteQuery;
private __gshared size_t gPaletteSelectedIndex;
private __gshared bool gPaletteActive;
private __gshared bool gPaletteFocusPending;

private void paletteOpen()
{
    gPaletteQuery = "";
    gPaletteSelectedIndex = 0;
    gPaletteActive = true;
    gPaletteFocusPending = true;
}

private void paletteClose()
{
    gPaletteActive = false;
    NotificationPopup.instance().close();
}

// Collect all registered commands across AllCommandMaps
private Command[] collectAllCommands()
{
    Command[] arr;
    static foreach (AA; AllCommandMaps) {
        foreach (k, v; AA) {
            if (v !is null) arr ~= v;
        }
    }
    return arr;
}

/// Show a searchable list and execute on Enter.
class ListCommandCommand : ExCommand!()
{
    this() { super(_("Show Command Palette.")); }

    override void run(Context ctx)
    {
        auto self = this; // capture for exclusion
        paletteOpen();

        NotificationPopup.instance().popup((ImGuiIO* io) {
            if (!gPaletteActive) return; // closed

            // Input field
            // Width: half of viewport area width
            float halfW = io.DisplaySize.x * 0.5f;
            igPushItemWidth(halfW);
            if (gPaletteFocusPending) {
                igSetKeyboardFocusHere(0);
                gPaletteFocusPending = false;
            }
            bool submitted = incInputText("PALETTE_QUERY", gPaletteQuery, ImGuiInputTextFlags.EnterReturnsTrue);
            igPopItemWidth();

            // Gather and filter commands by label substring (case-insensitive)
            string q = gPaletteQuery.toLower;
            Command[] all = collectAllCommands();

            Command[] filtered;
            foreach (c; all) {
                if (c is null) continue;
                if (c is self) continue; // exclude self
                auto lbl = c.label();
                if (q.length == 0 || canFind(lbl.toLower, q)) {
                    filtered ~= c;
                }
            }

            // Adjust selection bounds
            if (gPaletteSelectedIndex >= filtered.length) {
                gPaletteSelectedIndex = filtered.length > 0 ? filtered.length - 1 : 0;
            }

            // Navigation keys
            if (igIsKeyPressed(ImGuiKey.DownArrow, true)) {
                if (filtered.length > 0 && gPaletteSelectedIndex + 1 < filtered.length)
                    ++gPaletteSelectedIndex;
            }
            if (igIsKeyPressed(ImGuiKey.UpArrow, true)) {
                if (filtered.length > 0 && gPaletteSelectedIndex > 0)
                    --gPaletteSelectedIndex;
            }
            // Close on Escape
            if (igIsKeyPressed(ImGuiKey.Escape)) {
                paletteClose();
                return;
            }

            // Results list with scrolling when exceeding viewport height
            float lineH = igGetTextLineHeightWithSpacing();
            float estimated = filtered.length * lineH;
            float maxListH = io.DisplaySize.y * 0.8f;
            float listH = estimated < maxListH ? estimated : maxListH;
            if (listH < lineH * 3 && filtered.length > 0) listH = lineH * filtered.length; // minimal fit

            if (igBeginChild("PALETTE_RESULTS", ImVec2(halfW, listH), true, ImGuiWindowFlags.AlwaysVerticalScrollbar)) {
                import nijigenerate.core.shortcut : ngShortcutFor;
                foreach (i, c; filtered) {
                    auto lbl = c.label();
                    bool selected = (i == gPaletteSelectedIndex);
                    if (igSelectable(lbl.toStringz, selected)) {
                        gPaletteSelectedIndex = i;
                        // Execute on mouse selection
                        executeCommand(c);
                        igEndChild();
                        return;
                    }
                    // Render shortcut at right end if assigned
                    auto sc = ngShortcutFor(c);
                    if (sc.length) {
                        import std.string : toStringz;
                        ImVec2 sz; igCalcTextSize(&sz, sc.toStringz);
//                        ImVec2 rmin, rmax; igGetWindowContentRegionMin(&rmin); igGetWindowContentRegionMax(&rmax);
                        ImVec2 rmax = incAvailableSpace();
                        auto style = igGetStyle();
                        // account for vertical scrollbar reservation and right padding
                        float rightX = rmax.x - sz.x - style.ItemInnerSpacing.x - style.ScrollbarSize;
                        float curX = igGetCursorPosX();
                        float off = rightX - curX;
                        if (off > 0) igSameLine(0, off);
                        igText(sc.toStringz);
                    }
                }
            }
            igEndChild();

            // Execute on Enter
            if (submitted || igIsKeyPressed(ImGuiKey.Enter)) {
                if (filtered.length > 0) {
                    auto c = filtered[min(gPaletteSelectedIndex, filtered.length - 1)];
                    executeCommand(c);
                    return;
                }
            }

            // Close on Escape or click outside the palette window
            if (igIsKeyPressed(ImGuiKey.Escape)) {
                paletteClose();
                return;
            }
            // Close on outside click only if not interacting with any item in this frame
            if (igIsMouseClicked(ImGuiMouseButton.Left) &&
                !(igIsWindowHovered(ImGuiHoveredFlags.ChildWindows) || igIsAnyItemHovered())) {
                paletteClose();
                return;
            }
        }, -1); // infinite until closed
    }

private:
    void executeCommand(Command c)
    {
        import nijigenerate.core.shortcut.base : ngBuildExecutionContext; // wrapper to build Context
        auto ctx = ngBuildExecutionContext();
        if (c !is null && c.runnable(ctx)) {
            c.run(ctx);
            paletteClose();
        } else {
            // Keep palette open if command cannot run in current context
            // Optionally, we could show a status message here.
        }
    }
}
