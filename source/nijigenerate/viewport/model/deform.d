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
import nijigenerate.core.dbg;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.project;
import nijigenerate.widgets.button;
import nijigenerate.widgets.tooltip;
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

        if (incShowDepthBones) {
            ExDepthBone selectedDepthBone = null;
            foreach (node; incSelectedNodes()) {
                if (auto bone = cast(ExDepthBone)node) {
                    selectedDepthBone = bone;
                    break;
                }
            }
            foreach (root; findDepthRoots()) {
                drawDepthBones(root, findDepthRoot(selectedDepthBone) is root ? selectedDepthBone : null);
            }
        }
    }

    void drawDepthBones(ExDepthRigRoot root, ExDepthBone selectedBone = null) {
        if (root is null) return;
        Vec3Array lines;
        Vec3Array selectedLines;
        Vec3Array points;
        Vec3Array selectedPoints;
        auto rootToLocal = root.transform.matrix.inverse;

        vec3 bonePoint(ExDepthBone bone) {
            auto world = bone.transform.translation;
            return (rootToLocal * vec4(world.x, world.y, world.z, 1)).xyz;
        }

        foreach (bone; root.depthBones()) {
            auto point = bonePoint(bone);
            if (bone is selectedBone) {
                selectedPoints ~= point;
            } else {
                points ~= point;
            }

            if (auto parentBone = cast(ExDepthBone)bone.parent) {
                auto parentPoint = bonePoint(parentBone);
                if (bone is selectedBone || parentBone is selectedBone) {
                    selectedLines ~= parentPoint;
                    selectedLines ~= point;
                } else {
                    lines ~= parentPoint;
                    lines ~= point;
                }
            }
        }
        if (lines.length > 0) {
            inDbgSetBuffer(lines);
            inDbgDrawLines(vec4(0.55, 0.75, 1.0, 1), root.transform.matrix);
        }
        if (selectedLines.length > 0) {
            inDbgSetBuffer(selectedLines);
            inDbgDrawLines(vec4(1.0, 0.9, 0.2, 1), root.transform.matrix);
        }
        if (points.length > 0) {
            inDbgPointsSize(4);
            inDbgSetBuffer(points);
            inDbgDrawPoints(vec4(0.55, 0.75, 1.0, 1), root.transform.matrix);
        }
        if (selectedPoints.length > 0) {
            inDbgPointsSize(10);
            inDbgSetBuffer(selectedPoints);
            inDbgDrawPoints(vec4(1.0, 0.9, 0.2, 1), root.transform.matrix);
            inDbgPointsSize(4);
        }
    }

    ExDepthRigRoot findDepthRoot(ExDepthBone bone) {
        Node cursor = bone;
        while (cursor !is null) {
            if (auto root = cast(ExDepthRigRoot)cursor) return root;
            cursor = cursor.parent;
        }
        return null;
    }

    ExDepthRigRoot[] findDepthRoots() {
        ExDepthRigRoot[] roots;
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return roots;

        void visit(Node node) {
            if (node is null) return;
            if (auto root = cast(ExDepthRigRoot)node) roots ~= root;
            foreach (child; node.children) visit(child);
        }

        visit(puppet.root);
        return roots;
    }

    override
    void drawTools() {
        if (editor) {
            editor.viewportTools();
        }
    }

    override
    void drawOptions() {
        if(incBeginDropdownMenu("GIZMOS", "")) {
            if (incButtonColored("\ue8ef", ImVec2(0, 0), incShowDepthBones ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                incShowDepthBones = !incShowDepthBones;
            }
            incTooltip(incShowDepthBones ? _("Hide Depth Bones") : _("Show Depth Bones"));
            incEndDropdownMenu();
        }

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
