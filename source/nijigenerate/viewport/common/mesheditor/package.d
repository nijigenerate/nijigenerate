module nijigenerate.viewport.common.mesheditor;

/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Authors:
    - Luna Nielsen
    - Asahi Lina
*/
import i18n;
public import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.common.mesheditor.tools;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.viewport.model.deform;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.windows.flipconfig;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.utils.transform;
import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
import std.stdio;
import std.format;
import std.string;

class IncMeshEditor {
protected:
    IncMeshEditorOne[Node] editors;
    bool previewTriangulate = false;
    bool mirrorHoriz = false;
    bool mirrorVert = false;
    VertexToolMode toolMode = VertexToolMode.Points;

public:
    bool deformOnly;

    this(bool deformOnly) {
        this.deformOnly = deformOnly;
    }

    void setTarget(Node target) {
        if (target is null) {
        } else {
            addTarget(target);
        }
    }

    IncMeshEditorOne getEditorFor(Node drawing) {
        if (drawing in editors)
            return editors[drawing];
        return null;
    }

    abstract void addTarget(Node target);

    abstract void setTargets(Node[] targets);

    void removeTarget(Node target) {
        if (target in editors)
            editors.remove(target);
    }

    Node[] getTargets() {
        return editors.keys();
    }

    void refreshMesh() {
        foreach (drawing, editor; editors) {
            editor.refreshMesh();
        }
    }

    void resetMesh() {
        foreach (drawing, editor; editors) {
            editor.resetMesh();
        }
    }

    void applyPreview() {
        foreach (drawing, editor; editors) {
            editor.applyPreview();
        }
    }

    void applyToTarget() {
        foreach (drawing, editor; editors) {
            editor.applyToTarget();
        }
    }

    bool update(ImGuiIO* io, Camera camera) {
        bool result = false;
        incActionPushGroup();
        int[] actions;
        foreach (drawing, editor; editors) {
            actions ~= editor.peek(io, camera);
        }
        int action = 0;
        if (editors.keys().length > 0)
            action = editors[editors.keys()[0]].unify(actions);
        foreach (drawing, editor; editors) {
            result = editor.update(io, camera, action) || result;
        }
        foreach (drawing, editor; editors) {
            editor.getTarget().notifyChange(editor.getTarget(), NotifyReason.AttributeChanged);
        }
        incActionPopGroup();
        return result;
    }


    void draw(Camera camera) {
        foreach (drawing, editor; editors) {
            editor.draw(camera);
        }
    }

    void setToolMode(VertexToolMode toolMode) {
        this.toolMode = toolMode;
        foreach (drawing, editor; editors) {
            editor.setToolMode(toolMode);
        }
    }

    VertexToolMode getToolMode() {
        return toolMode;
    }

    void viewportTools() {
        igSetWindowFontScale(1.30);
            igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(1, 1));
            igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(8, 10));
                auto info = incGetToolInfo();
                foreach (i; info) {
                    if (i.viewportTools(deformOnly, getToolMode(), editors)) {
                        toolMode = i.mode();
                    }
                }

            igPopStyleVar(2);
        igSetWindowFontScale(1);
    }

    void displayGroupIds() {
        // Show group Id
        if (auto drawableEditor = cast(IncMeshEditorOneDrawable)editors.values[0]) {
            if (editors.length > 0 && drawableEditor.getMesh().maxGroupId > 1) {
                auto editor = cast(IncMeshEditorOneDrawable)editors.values[0];
                igSetWindowFontScale(1.30);
                    igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(10, 1));
                    igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(8, 10));

                        if (incButtonColored("All", ImVec2(0, 0), editor.getGroupId() == 0 ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                            foreach (e; editors) {
                                e.setGroupId(0);
                            }
                        }
                        igSameLine();
                        foreach (i; 1..1+editor.getMesh().maxGroupId) {
                            if (incButtonColored("%d".format(i).toStringz, ImVec2(0, 0), i == editor.getGroupId() ? colorUndefined: ImVec4(0.6, 0.6, 0.6, 1))) {
                                foreach (e; editors) {
                                    e.setGroupId(i);
                                }
                            }
                            igSameLine();
                        }

                    igPopStyleVar(2);
                igSetWindowFontScale(1);
            }
        }
    }

    void displayToolOptions() {
        foreach (i; incGetToolInfo()) {
            if (toolMode == i.mode) {
                i.displayToolOptions(deformOnly, toolMode, editors);
            }
        }
    }

    void resetSelection() {
        auto param = incArmedParameter();
        auto cParamPoint = param.findClosestKeypoint();

        ParameterBinding[] bindings = [];
        foreach (drawing, editor; editors) {
            bindings ~= param.getOrAddBinding(drawing, "deform");
        }

        auto action = new ParameterChangeBindingsValueAction("reset selection", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach (drawing, editor; editors) {
            auto binding = cast(DeformationParameterBinding)(param.getOrAddBinding(drawing, "deform"));
            assert (binding !is null);

            void clearValue(ref Deformation val) {
                // Reset deformation to identity, with the right vertex count
                if (Drawable d = cast(Drawable)drawing) {
                    val.vertexOffsets.length = d.vertices.length;
                    foreach(i; 0..d.vertices.length) {
                        if (editor.selected.countUntil(i) >= 0)
                            val.vertexOffsets[i] = vec2(0);
                    }
                }
            }
            clearValue(binding.values[cParamPoint.x][cParamPoint.y]);
            binding.getIsSet()[cParamPoint.x][cParamPoint.y] = true;

        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }

    void flipSelection() {
        auto param = incArmedParameter();
        auto cParamPoint = param.findClosestKeypoint();

        ParameterBinding[] bindings = [];
        foreach (drawing, editor; editors) {
            bindings ~= param.getOrAddBinding(drawing, "deform");
        }

        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("Flip selection horizontaly from mirror", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach (drawing, editor; editors) {
            auto binding = cast(DeformationParameterBinding)(param.getOrAddBinding(drawing, "deform"));
            assert (binding !is null);

            Node target = cast(Node)binding.getTarget().node;
            if (target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getName(), false);

                if (true)
                    incBindingAutoFlip(binding, targetBinding, cParamPoint, 0, true, &editor.selected);
                else
                    incBindingAutoFlip(targetBinding, binding, cParamPoint, 0, true, &editor.selected);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();        
    }

    void popupMenu() {
        bool selected = false;
        foreach (drawing, editor; editors) {
            if (editor.selected.length > 0) {
                selected = true;
                break;
            }
        }

        if (selected) {
            if (igMenuItem(__("Reset selected"), "", false, true)) {
                resetSelection();
            }
            if (igMenuItem(__("Flip selected from mirror"), "", false, true)) {
                flipSelection();
            }
        }
    }

    void setMirrorHoriz(bool mirrorHoriz) {
        this.mirrorHoriz = mirrorHoriz;
        foreach (e; editors) {
            e.mirrorHoriz = mirrorHoriz;
        }
    }

    bool getMirrorHoriz() {
        return mirrorHoriz;
    }

    void setMirrorVert(bool mirrorVert) {
        this.mirrorVert = mirrorVert;
        foreach (e; editors) {
            e.mirrorVert = mirrorVert;
        }
    }

    bool getMirrorVert() {
        return mirrorVert;
    }

    void setPreviewTriangulate(bool previewTriangulate) {
        this.mirrorVert = mirrorVert;
        foreach (e; editors) {
            e.previewTriangulate = previewTriangulate;
        }
    }

    bool getPreviewTriangulate() {
        return previewTriangulate;
    }

    bool previewingTriangulation() {
        foreach (e; editors) {
            if (!e.previewingTriangulation())
                return false;
        }
        return true;
    }

}

