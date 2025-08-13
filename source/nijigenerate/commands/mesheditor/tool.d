module nijigenerate.commands.mesheditor.tool;

import nijigenerate.commands.base;
import nijigenerate.viewport.model.deform : incViewportModelDeformGetEditor;
static import nijigenerate.viewport.common.mesheditor.tools.enums;
alias VertexToolMode = nijigenerate.viewport.common.mesheditor.tools.enums.VertexToolMode;
// incVertexViewportGetEditor may not exist on older commits; import if available
static import nijigenerate.viewport.vertex;

class SelectToolModeCommand : ExCommand!(TW!(VertexToolMode, "mode", "Tool mode")) {
    this(VertexToolMode mode) { super("Select mesh editor tool mode", mode); }
    override void run(Context ctx) {
        auto editor = incViewportModelDeformGetEditor();
        static if (__traits(compiles, { nijigenerate.viewport.vertex.incVertexViewportGetEditor(); })) {
            if (editor is null) editor = nijigenerate.viewport.vertex.incVertexViewportGetEditor();
        }
        if (editor is null) return;
        // Only set current mode indicator; UI flow performs actual setup
        editor.setToolModeOnly(mode);
    }
}

enum MeshEditorCommand {
    SelectToolMode,
}

Command[MeshEditorCommand] commands;
private {
    static this() {
        mixin(registerCommand!(MeshEditorCommand.SelectToolMode, VertexToolMode.Points));
    }
}
