module nijigenerate.commands.mesheditor.tool;

import nijigenerate.commands.base;
import nijigenerate.viewport.model.deform : incViewportModelDeformGetEditor;
static import nijigenerate.viewport.common.mesheditor.tools.enums;
alias VertexToolMode = nijigenerate.viewport.common.mesheditor.tools.enums.VertexToolMode;
// incVertexViewportGetEditor may not exist on older commits; import if available
static import nijigenerate.viewport.vertex;
// Access ToolInfo list to pre-register at startup
import nijigenerate.viewport.common.mesheditor.tools : incGetToolInfo;

class SelectToolModeCommand : ExCommand!(TW!(VertexToolMode, "mode", "Tool mode")) {
    this(VertexToolMode mode) { super("Select mesh editor tool mode", mode); }
    override void run(Context ctx) {
        auto editor = incViewportModelDeformGetEditor();
        static if (__traits(compiles, { nijigenerate.viewport.vertex.incVertexViewportGetEditor(); })) {
            if (editor is null) editor = nijigenerate.viewport.vertex.incVertexViewportGetEditor();
        }
        if (editor is null) return;
        // Only set current mode indicator; UI flow performs actual setup
        editor.setToolMode(mode);
    }

    override string label() {
        // Enrich label with tool description for better UI listing
        foreach (info; incGetToolInfo()) {
            if (info.mode() == mode) {
                return "Select Tool: " ~ info.description();
            }
        }
        import std.conv : to;
        return "Select Tool: " ~ mode.to!string;
    }
}

//
// Dynamic, per-mode command registry
// - We keep commands keyed by VertexToolMode (enum), but
//   the concrete set of modes to register is only known at runtime
//   from ToolInfo. We therefore register lazily on first use.
//
Command[VertexToolMode] selectToolModeCommands;

/// Ensure a SelectToolModeCommand is registered for the given mode
Command ensureSelectToolModeCommand(VertexToolMode mode)
{
    auto p = mode in selectToolModeCommands;
    if (p) return *p;
    auto c = cast(Command) new SelectToolModeCommand(mode);
    selectToolModeCommands[mode] = c;
    return c;
}

private:
// Template-based init for VertexToolMode
void ngInitCommands(T)() if (is(T == VertexToolMode))
{
    foreach (info; incGetToolInfo()) {
        ensureSelectToolModeCommand(info.mode());
    }
}
