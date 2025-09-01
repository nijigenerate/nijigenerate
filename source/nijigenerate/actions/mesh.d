module nijigenerate.actions.mesh;

import nijigenerate.core.actionstack;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.common.mesheditor.tools;
import nijigenerate.viewport.vertex;
import nijigenerate.actions;
import nijigenerate;
import nijilive;
import std.format;
import std.range;
import i18n;
//import std.stdio;
import std.algorithm;

/**
    Action for change of binding values at once
*/
abstract class MeshAction  : LazyBoundAction {
    string name;
    bool dirty;
    IncMeshEditorOne editor;
    IncMesh mesh;
    SubToolMode[] oldSubToolMode;
    SubToolMode[] newSubToolMode;

    struct Connection {
        MeshVertex* v1;
        MeshVertex* v2;
    };

    bool undoable = true;

    alias self = editor;

    this(string name, IncMeshEditorOne editor, IncMesh mesh, void delegate() update = null) {
        this.name = name;
        this.editor = editor;
        this.mesh = mesh;
        this.clear();

        auto filterTargets = self ? self.getFilterTargets(): [];
        if (filterTargets.length > 0) {
            oldSubToolMode = filterTargets.map!(t=>(cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode).array();
            import std.stdio;
            writefln("MeshAction: capture oldSubToolMode=%s", oldSubToolMode);
        }

        if (update !is null) {
            update();
            this.updateNewState();
        }
    }

    void markAsDirty() { dirty = true; }

    void updateNewState() {
        auto filterTargets = self ? self.getFilterTargets(): [];
        if (filterTargets.length > 0) {
            newSubToolMode = filterTargets.map!(t=>(cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode).array();
            import std.stdio;
            writefln("MeshAction: capture newSubToolMode=%s", newSubToolMode);
        }
    }

    void clear() {
        this.dirty = false;
    }

    /**
        Describe the action
    */
    string describe() {
        return _("Mesh %s change is restored.").format(name);
    }

    /**
        Describe the action
    */
    string describeUndo() {
        return _("Mesh %s was edited.").format(name);
    }

    /**
        Gets name of this action
    */
    string getName() {
        return this.stringof;
    }

    override
    void rollback() {
        auto filterTargets = self ? self.getFilterTargets(): [];
        if (filterTargets.length > 0) {
            foreach (i, t; filterTargets) {
                (cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode = oldSubToolMode[i];
            }
            import std.stdio;
            writefln("MeshAction: undo.mode=%s", oldSubToolMode);
        }
    }

    override
    void redo() {
        auto filterTargets = self ? self.getFilterTargets(): [];
        if (filterTargets.length > 0) {
            foreach (i, t; filterTargets) {
                (cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode = newSubToolMode[i];
            }
            import std.stdio;
            writefln("MeshAction: redo.mode=%s", newSubToolMode);
        }
    }

    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
};

class MeshConnectAction  : MeshAction {
    MeshVertex*[][MeshVertex*] connected;

    this(string name, IncMeshEditorOne editor, IncMesh mesh, void delegate() update = null) {
        super(name, editor, mesh, update);
    }

    void connect(MeshVertex* vertex, MeshVertex* other) {
        if (vertex !in connected || connected[vertex].countUntil(other) < 0)
            connected[vertex] ~= other;
        vertex.connect(other);
        dirty = true;
    }

    override
    void markAsDirty() { dirty = true; }

    override
    void updateNewState() {
        super.updateNewState();
    }

    override
    void clear() {
        connected.clear();
        super.clear();
    }

    /**
        Rollback
    */
    override
    void rollback() {
        if (undoable) {
            super.rollback();
            foreach (v, c; connected) {
                auto vertex = cast(MeshVertex*)v;
                foreach (other; c) {
                    vertex.disconnect(other);
                }
            }
            undoable = false;
            editor.refreshMesh();
        }
    }

    /**
        Redo
    */
    override
    void redo() {
        if (!undoable) {
            super.redo();
            foreach (v, c; connected) {
                auto vertex = cast(MeshVertex*)v;
                foreach (other; c) {
                    vertex.connect(other);
                }
            }
            undoable = true;
            editor.refreshMesh();
        }
    }

    /**
        Describe the action
    */
    override
    string describe() {
        return _("%s: vertex was translated.").format(name);
    }

    /**
        Describe the action
    */
    override
    string describeUndo() {
        return _("%s: vertex was translated.").format(name);
    }

    /**
        Gets name of this action
    */
    override
    string getName() {
        return this.stringof;
    }
};


class MeshDisconnectAction  : MeshAction {
    MeshVertex*[][MeshVertex*] connected;

    this(string name, IncMeshEditorOne editor, IncMesh mesh, void delegate() update = null) {
        super(name, editor, mesh, update);
    }

    void disconnect(MeshVertex* vertex, MeshVertex* other) {
        if (vertex !in connected || connected[vertex].countUntil(other) < 0)
            connected[vertex] ~= other;
        vertex.disconnect(other);
        dirty = true;
    }

    override
    void markAsDirty() { dirty = true; }

    override
    void updateNewState() {
        super.updateNewState();
    }

    override
    void clear() {
        connected.clear();
        super.clear();
    }

    /**
        Rollback
    */
    override
    void rollback() {
        if (undoable) {
            foreach (v, c; connected) {
                auto vertex = cast(MeshVertex*)v;
                foreach (other; c) {
                    vertex.connect(other);
                }
            }
            undoable = false;
            editor.refreshMesh();
        }
    }

    /**
        Redo
    */
    override
    void redo() {
        if (!undoable) {
            foreach (v, c; connected) {
                auto vertex = cast(MeshVertex*)v;
                foreach (other; c) {
                    vertex.disconnect(other);
                }
            }
            undoable = true;
            editor.refreshMesh();
        }
    }

    /**
        Describe the action
    */
    override
    string describe() {
        return _("%s: vertex was translated.").format(name);
    }

    /**
        Describe the action
    */
    override
    string describeUndo() {
        return _("%s: vertex was translated.").format(name);
    }

    /**
        Gets name of this action
    */
    override
    string getName() {
        return this.stringof;
    }
};