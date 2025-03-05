module nijigenerate.viewport.model.mesheditor.editor;

/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Authors:
    - Luna Nielsen
    - Asahi Lina
*/
public import nijigenerate.viewport.common.mesheditor;
public import nijigenerate.viewport.model.mesheditor.deformable;
public import nijigenerate.viewport.model.mesheditor.drawable;
public import nijigenerate.viewport.model.mesheditor.node;

import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import nijigenerate.viewport.common.mesheditor.tools.enums;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
import std.stdio;
import std.format;
import std.string;
import i18n;

class DeformMeshEditor : IncMeshEditor {
public:
    this() {
        super(true);
    }

    override
    void addTarget(Node target) {
        if (target in editors)
            return;
        IncMeshEditorOne subEditor;

        if (auto drawable = cast(Drawable)target) { 
            subEditor = new IncMeshEditorOneDrawableDeform();
        } else if (auto deformable = cast(Deformable)target) {
            subEditor = new IncMeshEditorOneDeformableDeform();
            if (cast(PathDeformer)deformable) {
                subEditor.toolMode = VertexToolMode.BezierDeform;
                toolMode = VertexToolMode.BezierDeform;
            }
        }

        subEditor.setTarget(target);
        subEditor.mirrorHoriz = mirrorHoriz;
        subEditor.mirrorVert  = mirrorVert;
        subEditor.previewTriangulate = previewTriangulate;
        editors[target] = subEditor;
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
                    subEditor = new IncMeshEditorOneDrawableDeform();
                } else if (auto deformable = cast(Deformable)t) {
                    subEditor = new IncMeshEditorOneDeformableDeform();
                    if (cast(PathDeformer)deformable) {
                        subEditor.toolMode = VertexToolMode.BezierDeform;
                        toolMode = VertexToolMode.BezierDeform;
                    }
                } else {
                    subEditor = new IncMeshEditorOneNode(deformOnly);
                }
                subEditor.setTarget(t);
                subEditor.mirrorHoriz = mirrorHoriz;
                subEditor.mirrorVert  = mirrorVert;
                subEditor.previewTriangulate = previewTriangulate;
                newEditors[t] = subEditor;
            }
        }
        editors = newEditors;
    }
}

