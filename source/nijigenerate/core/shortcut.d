module nijigenerate.core.shortcut;

import nijigenerate.commands;
import nijigenerate.core.input;
import nijigenerate.project;
import bindbc.imgui;

void incHandleShortcuts() {
    auto io = igGetIO();
    Context ctx = new Context();
    ctx.puppet = incActivePuppet();
    if (incSelectedNodes().length > 0)
        ctx.nodes = incSelectedNodes();

    if (incShortcut("Ctrl+N")) cmd!(FileCommand.NewFile)(ctx);
    if (incShortcut("Ctrl+O")) cmd!(FileCommand.ShowOpenFileDialog)(ctx);
    if (incShortcut("Ctrl+S")) cmd!(FileCommand.ShowSaveFileDialog)(ctx);
    if (incShortcut("Ctrl+Shift+S")) cmd!(FileCommand.ShowSaveFileAsDialog)(ctx);

    if (incShortcut("Ctrl+Shift+Z", true)) cmd!(EditCommand.Redo)(ctx);
    else if (incShortcut("Ctrl+Z", true)) cmd!(EditCommand.Undo)(ctx);

    if (incShortcut("Ctrl+C", true)) cmd!(NodeCommand.CopyNode)(ctx);
    else if (incShortcut("Ctrl+V", true)) cmd!(NodeCommand.PasteNode)(ctx);
}
