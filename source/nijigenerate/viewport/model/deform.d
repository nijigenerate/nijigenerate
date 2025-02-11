/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport.model.deform;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.base;
import nijigenerate.widgets.tooltip;
import nijigenerate.core.input;
import nijilive.core.dbg;
import nijigenerate.project;
import nijilive;
import bindbc.imgui;
import i18n;

class DeformationViewport : Viewport {
protected:
    Drawable selected = null;
public:
    IncMeshEditor editor;

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

        Parameter param = incArmedParameter();

        if (editor.update(io, camera)) {
            foreach (d; incSelectedNodes()) {
                if (auto deformable = cast(Deformable)d) {
                    auto deform = cast(DeformationParameterBinding)param.getOrAddBinding(deformable, "deform");
                    deform.update(param.findClosestKeypoint(), editor.getEditorFor(deformable).getOffsets());
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
        paramValueChanged();
    }


    void paramValueChanged() {
        if (Parameter param = incArmedParameter()) {
            auto drawables = incSelectedNodes();

            if (!editor) {
                if (drawables && drawables.length > 0) {
                    editor = new IncMeshEditor(true);
                    editor.setTargets(drawables);
                } else
                    return;
            } else {
                editor.setTargets(drawables);
                editor.resetMesh();
            }

            foreach (node; editor.getTargets()) {
                auto e = editor.getEditorFor(node);
                DeformationParameterBinding deform = null;
                if (auto deformable = cast(Deformable)node)
                    deform = cast(DeformationParameterBinding)param.getBinding(deformable, "deform");
                if (e !is null) {
                    if (deform !is null) {
                        auto binding = deform.getValue(param.findClosestKeypoint());
                        e.applyOffsets(binding.vertexOffsets);
                    }
                    e.adjustPathTransform();
                }
            }
        } else {
            editor = null;
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

