/*
    Undo/redo actions for in-progress depth editor values.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.mesheditor.action;

import i18n;
import nijigenerate.actions;
import nijigenerate.viewport.depth.mesheditor.node;
import nijigenerate.viewport.depth.mesheditor.editor;
import nijigenerate.viewport.depth.tools.operation;

class DepthOperationListChangeAction : LazyBoundAction {
private:
    DepthMeshEditor editor;
    DepthMeshEditorOne target;
    DepthOperation[] oldOperations;
    DepthOperation[] newOperations;

public:
    this(DepthMeshEditor editor, DepthMeshEditorOne target) {
        this.editor = editor;
        this.target = target;
        this.oldOperations = editor.copyOperations(target);
    }

    override
    void updateNewState() {
        newOperations = editor.copyOperations(target);
    }

    override
    void clear() { }

    override
    void rollback() {
        editor.replaceOperations(target, oldOperations);
    }

    override
    void redo() {
        editor.replaceOperations(target, newOperations);
    }

    override
    string describe() {
        return _("Changed depth operations");
    }

    override
    string describeUndo() {
        return _("Depth operations were changed");
    }

    override
    string getName() {
        return this.stringof;
    }

    override bool merge(Action other) { return false; }
    override bool canMerge(Action other) { return false; }
}
