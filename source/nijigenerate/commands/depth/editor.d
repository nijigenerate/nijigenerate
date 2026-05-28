module nijigenerate.commands.depth.editor;

import i18n;
import nijigenerate.commands.base;
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.viewport.depth.mesheditor.action;
import nijigenerate.viewport.depth.mesheditor.editor;
import nijigenerate.viewport.depth.mesheditor.node;
import nijigenerate.viewport.depth.tools.operation;

enum DepthEditorOperationCommand {
    AddEditorDepthOp,
    UpdateEditorDepthOp,
    RemoveEditorDepthOp,
}

Command[DepthEditorOperationCommand] commands;

private CommandResult replaceEditorDepthOpsWithUndo(
    DepthMeshEditor editorSet,
    DepthMeshEditorOne target,
    DepthOperation[] nextOperations,
    ptrdiff_t selectedIndex
) {
    if (editorSet is null || target is null) return CommandResult(false);

    auto action = new DepthOperationListChangeAction(editorSet, target);
    editorSet.replaceOperations(target, nextOperations);
    editorSet.selectOperation(target, selectedIndex);
    action.updateNewState();
    incActionPush(action);
    return CommandResult(true);
}

@McpHidden
class AddEditorDepthOpCommand : ExCommand!(
    TW!(DepthMeshEditor, "editorSet", "Depth editor session"),
    TW!(DepthMeshEditorOne, "target", "Depth editor target"),
    TW!(DepthOperation, "operation", "Depth operation"),
    TW!(int, "index", "Insertion index, or -1 to append")
) {
    this() { super(_("Add Editor Depth Operation"), _("Add one in-progress depth operation")); }

    override CommandResult run(Context ctx) {
        if (editorSet is null || target is null || operation is null) return CommandResult(false);

        auto list = editorSet.copyOperations(target);
        auto existingIndex = editorSet.indexOfOperationInstance(target, operation);
        if (existingIndex >= 0) {
            auto i = cast(size_t)existingIndex;
            list = list[0 .. i] ~ list[i + 1 .. $];
            if (index < 0) index = cast(int)i;
        }

        ptrdiff_t selectedIndex;
        if (index < 0 || index >= list.length) {
            list ~= operation;
            selectedIndex = cast(ptrdiff_t)list.length - 1;
        } else {
            auto i = cast(size_t)index;
            list = list[0 .. i] ~ [operation] ~ list[i .. $];
            selectedIndex = cast(ptrdiff_t)i;
        }

        return replaceEditorDepthOpsWithUndo(editorSet, target, list, selectedIndex);
    }
}

@McpHidden
class UpdateEditorDepthOpCommand : ExCommand!(
    TW!(DepthMeshEditor, "editorSet", "Depth editor session"),
    TW!(DepthMeshEditorOne, "target", "Depth editor target"),
    TW!(int, "index", "Operation index"),
    TW!(DepthOperation, "operation", "Replacement depth operation")
) {
    this() { super(_("Update Editor Depth Operation"), _("Replace one in-progress depth operation")); }

    override CommandResult run(Context ctx) {
        if (editorSet is null || target is null || operation is null || index < 0) return CommandResult(false);
        auto list = editorSet.copyOperations(target);
        if (index >= list.length) return CommandResult(false);
        list[cast(size_t)index] = operation;
        return replaceEditorDepthOpsWithUndo(editorSet, target, list, index);
    }
}

@McpHidden
class RemoveEditorDepthOpCommand : ExCommand!(
    TW!(DepthMeshEditor, "editorSet", "Depth editor session"),
    TW!(DepthMeshEditorOne, "target", "Depth editor target"),
    TW!(int, "index", "Operation index")
) {
    this() { super(_("Remove Editor Depth Operation"), _("Remove one in-progress depth operation")); }

    override CommandResult run(Context ctx) {
        if (editorSet is null || target is null || index < 0) return CommandResult(false);
        auto list = editorSet.copyOperations(target);
        if (index >= list.length) return CommandResult(false);
        auto i = cast(size_t)index;
        list = list[0 .. i] ~ list[i + 1 .. $];
        auto selectedIndex = list.length ? cast(ptrdiff_t)list.length - 1 : -1;
        return replaceEditorDepthOpsWithUndo(editorSet, target, list, selectedIndex);
    }
}

void ngInitCommands(T)() if (is(T == DepthEditorOperationCommand)) {
    auto addOp = new AddEditorDepthOpCommand();
    ngRegisterCommandMeta(addOp);
    commands[DepthEditorOperationCommand.AddEditorDepthOp] = addOp;

    auto updateOp = new UpdateEditorDepthOpCommand();
    ngRegisterCommandMeta(updateOp);
    commands[DepthEditorOperationCommand.UpdateEditorDepthOp] = updateOp;

    auto removeOp = new RemoveEditorDepthOpCommand();
    ngRegisterCommandMeta(removeOp);
    commands[DepthEditorOperationCommand.RemoveEditorDepthOp] = removeOp;
}
