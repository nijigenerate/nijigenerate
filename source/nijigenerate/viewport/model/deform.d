/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport.model.deform;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.widgets.tooltip;
import nijigenerate.core.input;
import nijilive.core.dbg;
//import nijigenerate.core;
import nijigenerate.project;
import nijilive;
import bindbc.imgui;
import i18n;

private {
    class ModelView {
        void onSelectionChanged(Node[] n) {
            editor = null;
            incViewportNodeDeformNotifyParamValueChanged();
        }

        void onArmedParameterChanged(Parameter param) {
            incViewportNodeDeformNotifyParamValueChanged();        
        }
    }
    IncMeshEditor editor;
    Drawable selected = null;
    ModelView view;

    static this() {
        view = new ModelView;
        ngRegisterProjectCallback((Project project) { 
            project.SelectionChanged.connect(&view.onSelectionChanged);
            project.ArmedParameterChanged.connect(&view.onArmedParameterChanged);
        });
    }
}

void incViewportNodeDeformNotifyParamValueChanged() {
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

//void incViewportModelDeformNodeSelectionChanged() {
//    editor = null;
//    incViewportNodeDeformNotifyParamValueChanged();
//}

void incViewportModelDeformUpdate(ImGuiIO* io, Camera camera, Parameter param) {
    if (!editor) return;

    if (editor.update(io, camera)) {
        foreach (d; incSelectedNodes()) {
            if (auto deformable = cast(Deformable)d) {
                auto deform = cast(DeformationParameterBinding)param.getOrAddBinding(deformable, "deform");
                deform.update(param.findClosestKeypoint(), editor.getEditorFor(deformable).getOffsets());
            }
        }
    }
}

void incViewportModelDeformDraw(Camera camera, Parameter param) {
    if (editor)
        editor.draw(camera);
}

void incViewportModelDeformTools() {
    if (editor) {
        editor.viewportTools();
    }
}

void incViewportModelDeformOptions() {
    if (editor) {
        editor.displayToolOptions();
    }
}

void incViewportModelDeformToolSettings() {

}

IncMeshEditor incViewportModelDeformGetEditor() {
    return editor;
}
