module nijigenerate.actions.mesh;

import nijigenerate.core.actionstack;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.vertex;
import nijigenerate.viewport;
import nijigenerate.actions;
import nijigenerate;
import nijilive;
import std.format;
import std.range;
import i18n;
import std.stdio;
import std.algorithm;

/**
    Action for change of binding values at once
*/
abstract class MeshAction  : LazyBoundAction {
    string name;
    bool dirty;
    IncMeshEditorOne editor;
    IncMesh mesh;

    struct Connection {
        MeshVertex* v1;
        MeshVertex* v2;
    };

    bool undoable = true;

    this(string name, IncMeshEditorOne editor, IncMesh mesh, void delegate() update = null) {
        this.name = name;
        this.editor = editor;
        this.mesh = mesh;
        this.clear();

        if (update !is null) {
            update();
            this.updateNewState();
        }
    }

    void markAsDirty() { dirty = true; }

    void updateNewState() {
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