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

class DepthEditorDepthChangeAction : LazyBoundAction {
private:
    DepthMeshEditor editor;
    DepthMeshEditorOne target;
    float[] oldDepths;
    float[] oldBaseDepths;
    DepthOperation[] oldOperations;
    bool oldDirectDepthDirty;
    float[] newDepths;
    float[] newBaseDepths;
    DepthOperation[] newOperations;
    bool newDirectDepthDirty;

public:
    this(DepthMeshEditor editor, DepthMeshEditorOne target) {
        this.editor = editor;
        this.target = target;
        this.oldDepths = target.copyEditorDepths();
        this.oldBaseDepths = target.baseDepths.dup;
        this.oldOperations = editor.copyOperations(target);
        this.oldDirectDepthDirty = editor.isDirectDepthDirty(target);
    }

    override
    void updateNewState() {
        newDepths = target.copyEditorDepths();
        newBaseDepths = target.baseDepths.dup;
        newOperations = editor.copyOperations(target);
        newDirectDepthDirty = editor.isDirectDepthDirty(target);
    }

    override
    void clear() { }

    override
    void rollback() {
        editor.replaceDepthState(target, oldDepths, oldBaseDepths, oldOperations, oldDirectDepthDirty);
    }

    override
    void redo() {
        editor.replaceDepthState(target, newDepths, newBaseDepths, newOperations, newDirectDepthDirty);
    }

    override
    string describe() {
        return _("Changed vertex depths");
    }

    override
    string describeUndo() {
        return _("Vertex depths were changed");
    }

    override
    string getName() {
        return this.stringof;
    }

    override bool merge(Action other) { return false; }
    override bool canMerge(Action other) { return false; }
}
