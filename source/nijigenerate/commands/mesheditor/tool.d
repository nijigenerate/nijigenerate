module nijigenerate.commands.mesheditor.tool;

import nijigenerate.commands.base;
import nijigenerate.viewport.model.deform : incViewportModelDeformGetEditor;
static import nijigenerate.viewport.common.mesheditor.tools.enums;
alias VertexToolMode = nijigenerate.viewport.common.mesheditor.tools.enums.VertexToolMode;
// incVertexViewportGetEditor may not exist on older commits; import if available
static import nijigenerate.viewport.vertex;
// Access ToolInfo list to pre-register at startup
import nijigenerate.viewport.common.mesheditor.tools : incGetToolInfo, ngGetToolInfoOf, ToolInfo;

class SelectToolModeCommand : ExCommand!(TW!(VertexToolMode, "mode", "Tool mode")) {
    ToolInfo info;
    this(VertexToolMode mode) { 
        import i18n;
        super(null, _("Select mesh editor tool mode"), mode); 
        _init();
    }
    override void run(Context ctx) {
        auto editor = incViewportModelDeformGetEditor();
        static if (__traits(compiles, { nijigenerate.viewport.vertex.incVertexViewportGetEditor(); })) {
            if (editor is null) editor = nijigenerate.viewport.vertex.incVertexViewportGetEditor();
        }
        if (editor is null) return;
        // Only set current mode indicator; UI flow performs actual setup
        editor.setToolMode(mode);
    }

    override bool runnable(Context ctx) {
        auto editor = incViewportModelDeformGetEditor();
        static if (__traits(compiles, { nijigenerate.viewport.vertex.incVertexViewportGetEditor(); })) {
            if (editor is null) editor = nijigenerate.viewport.vertex.incVertexViewportGetEditor();
        }
        if (editor is null) return false;
        if (info is null) return false;
        return info.canUse(editor.deformOnly, editor.getTargets());
    }

    void _init() {
        // Enrich label with tool description for better UI listing
        foreach (info; incGetToolInfo()) {
            if (info.mode() == mode) {
                _label = "Select Tool: " ~ info.description();
            }
        }
        import std.conv;
        _label = "Select Tool: " ~ mode.to!string;
        info = ngGetToolInfoOf(mode);
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

// Template-based init for VertexToolMode
void ngInitCommands(T)() if (is(T == nijigenerate.viewport.common.mesheditor.tools.enums.VertexToolMode)) {
    foreach (info; incGetToolInfo()) {
        ensureSelectToolModeCommand(info.mode());
    }
}
