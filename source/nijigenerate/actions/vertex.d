module nijigenerate.actions.vertex;

import nijigenerate.core.actionstack;
import nijigenerate.viewport.common.mesheditor.operations.deformable;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.core.math;
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
abstract class VertexAction  : LazyBoundAction {
    string name;
    bool dirty;
    IncMeshEditorOne editor;
    struct Connection {
        MeshVertex* v1;
        MeshVertex* v2;
    };


    bool undoable = true;

    this(string name, IncMeshEditorOne editor, void delegate() update = null) {
        this.name = name;
        this.editor = editor;
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

class VertexAddAction  : VertexAction {
    MeshVertex*[] vertices;
    this(string name, IncMeshEditorOne editor, void delegate() update = null) {
        super(name, editor, update);
    }

    void addVertex(MeshVertex* vertex) {
        vertices ~= vertex;
        editor.addMeshVertex(vertex);
        dirty = true;
    }

    override
    void markAsDirty() { dirty = true; }

    override
    void updateNewState() {
    }

    override
    void clear() {
        vertices.length = 0;
        super.clear();
    }

    /**
        Rollback
    */
    void rollback() {
        if (undoable) {
            foreach (v; vertices) {
                editor.removeMeshVertex(v);
            }
            undoable = false;
            editor.refreshMesh();
        }
    }

    /**
        Redo
    */
    void redo() {
        if (!undoable) {
            foreach (v; vertices) {
                editor.addMeshVertex(v);
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
        return _("%s: vertex was added.").format(name);
    }

    /**
        Describe the action
    */
    override
    string describeUndo() {
        return _("%s: vertex was removed.").format(name);
    }

    /**
        Gets name of this action
    */
    override
    string getName() {
        return this.stringof;
    }
};

class VertexRemoveAction  : VertexAction {
    MeshVertex*[] vertices;
    Connection[] connections;
    this(string name, IncMeshEditorOne editor, void delegate() update = null) {
        super(name, editor, update);
    }

    void removeVertex(MeshVertex* vertex, bool executeAction = true) {
        vertices ~= vertex;
        foreach (con; vertex.connections) {
            connections ~= Connection(vertex, con);
        }
        if (executeAction)
            editor.removeMeshVertex(vertex);
        dirty = true;
    }

    void removeVertices() {
        foreach (v; vertices) {
            editor.removeMeshVertex(v);
        }
    }

    override
    void markAsDirty() { dirty = true; }

    override
    void updateNewState() {
    }

    override
    void clear() {
        vertices.length = 0;
        super.clear();
    }

    /**
        Rollback
    */
    override
    void rollback() {
        if (undoable) {
            foreach (v; vertices) {
                editor.addMeshVertex(v);
            }
            foreach (c; connections) {
                c.v1.connect(c.v2);
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
            foreach (v; vertices) {
                editor.removeMeshVertex(v);
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
        return _("%s: vertex was removed.").format(name);
    }

    /**
        Describe the action
    */
    override
    string describeUndo() {
        return _("%s: vertex was added.").format(name);
    }

    /**
        Gets name of this action
    */
    override
    string getName() {
        return this.stringof;
    }
};


class VertexMoveAction  : VertexAction {
    struct Translation {
        vec2 original;
        vec2 translated;
    };
    Translation[MeshVertex*] translations;

    this(string name, IncMeshEditorOne editor, void delegate() update = null) {
        super(name, editor, update);
    }

    void moveVertex(MeshVertex* vertex, vec2 newPos) {
        if (vertex in translations) {
            translations[vertex].translated = newPos;
        } else {
            translations[vertex] = Translation(vertex.position, newPos);
        }
        editor.moveMeshVertex(vertex, newPos);
        dirty = true;
    }

    override
    void markAsDirty() { dirty = true; }

    override
    void updateNewState() {}

    override
    void clear() {
        translations.clear();
        super.clear();
    }

    /**
        Rollback
    */
    override
    void rollback() {
        if (undoable) {
            foreach (v, t; translations) {
                auto vertex = cast(MeshVertex*)v;
                vertex.position = t.original;
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
            foreach (v, t; translations) {
                auto vertex = cast(MeshVertex*)v;
                vertex.position = t.translated;
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


class VertexReorderAction  : VertexAction {
    MeshVertex*[] vertices;
    this(string name, IncMeshEditorOne editor, void delegate() update = null) {
        super(name, editor, update);
    }

    override
    void markAsDirty() { dirty = true; }

    override
    void updateNewState() {
    }

    /**
        Rollback
    */
    override
    void rollback() {
        if (undoable) {
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