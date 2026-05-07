/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport.model.deform;
import nijigenerate.viewport.model.mesheditor;
import nijigenerate.viewport.base;
import nijigenerate.core.input;
import nijigenerate.project;
import nijilive;
import bindbc.imgui;
import i18n;

class DeformationViewport : Viewport {
public:
    IncMeshEditor editor;
    Parameter parameter = null;

    override
    void draw(Camera camera) { 
        if (editor)
            editor.draw(camera);
    }

    override
    void drawTools() {
        if (editor) {
            editor.viewportTools();
        }
    }

    override
    void drawOptions() {
        if (editor) {
            editor.displayToolOptions();
        }
    }

    override
    void update(ImGuiIO* io, Camera camera) {
        if (!editor) return;

        if (editor.update(io, camera)) {
            foreach (d; incSelectedNodes()) {
                if (auto deformable = cast(Deformable)d) {
                    auto deform = cast(DeformationParameterBinding)parameter.getOrAddBinding(deformable, "deform");
                    deform.update(parameter.findClosestKeypoint(), editor.getEditorFor(deformable).getOffsets());
                }
            }
        }
    }

    override
    void selectionChanged(Node[] nodes) {
        editor = null;
        paramValueChanged();
    }
 
    override
    void armedParameterChanged(Parameter parameter) {
        this.parameter = parameter;
        paramValueChanged();
    }


    void paramValueChanged() {
        if (parameter) {
            auto drawables = incSelectedNodes();

            syncEditor(parameter, parameter.findClosestKeypoint(), drawables);
        } else {
            editor = null;
        }
    }

    void syncEditor(Parameter parameter, Node[] targets) {
        if (parameter is null) {
            syncEditor(parameter, vec2u(0, 0), targets);
        } else {
            syncEditor(parameter, parameter.findClosestKeypoint(), targets);
        }
    }

    void syncEditor(Parameter parameter, vec2u keyPoint, Node[] targets) {
        this.parameter = parameter;
        if (parameter is null) {
            editor = null;
            return;
        }
        if (targets.length == 0) return;

        if (!editor) {
            editor = new DeformMeshEditor();
        } else {
            foreach (node; editor.getTargets()) {
                if (auto e = editor.getEditorFor(node)) {
                    e.pushDeformAction();
                    e.forceResetAction();
                }
            }
        }

        editor.setTargets(targets);
        editor.resetMesh();

        foreach (node; editor.getTargets()) {
            auto e = editor.getEditorFor(node);
            DeformationParameterBinding deform = null;
            if (auto deformable = cast(Deformable)node)
                deform = cast(DeformationParameterBinding)parameter.getBinding(deformable, "deform");
            if (e !is null) {
                if (deform !is null) {
                    auto binding = deform.getValue(keyPoint);
                    e.applyOffsets(binding.vertexOffsets);
                }
                e.adjustPathTransform();
            }
        }
    }

    override
    void menu() {
        if (editor)
            editor.popupMenu();
    }
}



DeformationViewport incDeformationViewport() {
    if (auto modelView = cast(DelegationViewport)incViewport().subView) {
        return cast(DeformationViewport)modelView.subView;
    }
    return null;
}

void incViewportNodeDeformNotifyParamValueChanged() {
    if (auto view = incDeformationViewport)
        view.paramValueChanged();
}

IncMeshEditor incViewportModelDeformGetEditor() {
    if (auto view = incDeformationViewport)
        return view.editor;
    return null;
}

IncMeshEditor incViewportModelDeformSyncEditor(Parameter parameter, vec2u keyPoint, Node[] targets) {
    if (auto view = incDeformationViewport) {
        view.syncEditor(parameter, keyPoint, targets);
        return view.editor;
    }
    return null;
}
