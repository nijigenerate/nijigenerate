module nijigenerate.viewport.vertex.mesheditor.editor;

/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Authors:
    - Luna Nielsen
    - Asahi Lina
*/
import i18n;
public import nijigenerate.viewport.common.mesheditor;
public import nijigenerate.viewport.common.mesheditor.tools.enums;
public import nijigenerate.viewport.vertex.mesheditor.deformable;
public import nijigenerate.viewport.vertex.mesheditor.drawable;
//public import nijigenerate.viewport.vertex.mesheditor.node;
import nijigenerate.core.actionstack;
import nijigenerate.viewport.common.mesh;
import nijigenerate;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
//import std.stdio;
import std.format;
import std.string;

class VertexMeshEditor : IncMeshEditor {
private:
    void adjustToolModeForSelection() {
        bool anyGrid = false;
        bool anyPath = false;
        bool anyOther = false;

        foreach (node; editors.keys) {
            if (cast(GridDeformer)node) {
                anyGrid = true;
            } else if (cast(PathDeformer)node) {
                anyPath = true;
            } else {
                anyOther = true;
            }
        }

        auto current = getToolMode();

        if (anyGrid) {
            if (!anyPath && !anyOther) {
                if (current != VertexToolMode.Grid) {
                    setToolMode(VertexToolMode.Grid);
                }
            } else if (current == VertexToolMode.Grid) {
                if (anyPath && !anyOther) {
                    if (current != VertexToolMode.BezierDeform) {
                        setToolMode(VertexToolMode.BezierDeform);
                    }
                } else {
                    setToolMode(VertexToolMode.Points);
                }
            }
        } else if (anyPath && !anyOther) {
            if (current != VertexToolMode.BezierDeform) {
                setToolMode(VertexToolMode.BezierDeform);
            }
        }
    }

public:
    this() {
        super(false);
    }

    override
    void addTarget(Node target) {
        if (target in editors)
            return;
        IncMeshEditorOne subEditor;

        if (auto drawable = cast(Drawable)target) {
            incActionPushStack();
            subEditor = new IncMeshEditorOneFor!(Drawable, EditMode.VertexEdit)();
            float[][] axes;
            if (drawable.getMesh().vertices.isGrid(axes)) {
                subEditor.toolMode = VertexToolMode.Grid;
                toolMode           = VertexToolMode.Grid;
            }
        } else if (auto deformable = cast(Deformable)target) {
            incActionPushStack();
            subEditor = new IncMeshEditorOneFor!(Deformable, EditMode.VertexEdit)();
            if (cast(PathDeformer)deformable) {
                subEditor.toolMode = VertexToolMode.BezierDeform;
            } else if (cast(GridDeformer)deformable) {
                subEditor.toolMode = VertexToolMode.Grid;
            }
        }

        subEditor.setTarget(target);
        subEditor.mirrorHoriz = mirrorHoriz;
        subEditor.mirrorVert  = mirrorVert;
        subEditor.previewTriangulate = previewTriangulate;
        editors[target] = subEditor;
        adjustToolModeForSelection();
    }

    override
    void setTargets(Node[] targets) {
        IncMeshEditorOne[Node] newEditors;
        foreach (t; targets) {
            if (t in editors) {
                newEditors[t] = editors[t];
            } else {
                IncMeshEditorOne subEditor = null;
                if (auto drawable = cast(Drawable)t) {
                    incActionPushStack();
                    subEditor = new IncMeshEditorOneFor!(Drawable, EditMode.VertexEdit)();
                } else if (auto deformable = cast(Deformable)t) {
                    incActionPushStack();
                    subEditor = new IncMeshEditorOneFor!(Deformable, EditMode.VertexEdit)();
                    if (cast(GridDeformer)deformable) {
                        subEditor.toolMode = VertexToolMode.Grid;
                    }
                    if (cast(PathDeformer)deformable) {
                        subEditor.toolMode = VertexToolMode.BezierDeform;
                    }
                } else {
                    incActionPushStack();
                    subEditor = new IncMeshEditorOneFor!Node(deformOnly);
                }
                subEditor.setTarget(t);
                subEditor.mirrorHoriz = mirrorHoriz;
                subEditor.mirrorVert  = mirrorVert;
                subEditor.previewTriangulate = previewTriangulate;
                newEditors[t] = subEditor;
            }
        }
        editors = newEditors;
        adjustToolModeForSelection();
    }
}
