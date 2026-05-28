/*
    Depth mesh editor collection.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.mesheditor.editor;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.core.actionstack;
import nijigenerate.core.input;
import nijigenerate.commands : Context, cmd;
import nijigenerate.commands.depth.editor : DepthEditorOperationCommand;
import nijigenerate.commands.depth.map : DepthMapCommand, ngDepthOpToJson;
import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.exdepthops;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.mesheditor.action;
import nijigenerate.viewport.depth.mesheditor.node;
import nijigenerate.viewport.depth.renderer;
import nijigenerate.viewport.depth.tools.operation;
import nijigenerate.widgets.drag;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.format : format;
import std.algorithm : max;
import std.json : JSONValue;
import std.string : toStringz;

class DepthMeshEditor {
private:
    DepthMeshEditorOne[GridDeformer] editors;
    DepthOperation[][DepthMeshEditorOne] operations;
    DepthMeshEditorOne selectedOperationEditor;
    ptrdiff_t selectedOperationIndex = -1;
    DepthOperationHandle selectedOperationHandle = DepthOperationHandle.None;
    DepthOperationListChangeAction dragOperationAction;
    DepthOperation dragStartOperation;
    vec2 dragStartLocal;
    float dragStartMouseY;
    bool draggingOperation;
    DepthTextureMeshRenderer renderer;
    ActionStackScope actionScope;
    bool buildRenderResources;

    bool editable(Node node) {
        auto ok = cast(GridDeformer)node !is null;
        return ok;
    }

    void loadOperationsFromTarget(DepthMeshEditorOne editor) {
        if (editor is null) return;
        auto operated = cast(DepthOperationMappedNode)editor.getTarget();
        if (operated is null) return;

        auto saved = operated.copyDepthOps();
        if (saved.length == 0) return;

        DepthOperation[] loaded;
        foreach (op; saved) loaded ~= depthOperationFromExDepthOp(op);
        operations[editor] = loaded;

        // depth-ops are the editable source. Rebuild working depths from the
        // operation list so saved depths do not get applied a second time.
        editor.clearBaseDepths();
        recompute(editor);
    }

    void syncOperationsFromTarget(DepthMeshEditorOne editor) {
        if (editor is null) return;
        operations.remove(editor);
        loadOperationsFromTarget(editor);
    }

    JSONValue operationsToJson(DepthMeshEditorOne editor) {
        JSONValue saved = JSONValue.emptyArray;
        if (editor in operations) {
            foreach (op; operations[editor]) {
                saved.array ~= ngDepthOpToJson(toExDepthOp(op));
            }
        }
        return saved;
    }

public:
    bool commitOperationAdd(DepthMeshEditorOne editor, DepthOperation operation, int index = -1) {
        auto ctx = new Context();
        return cmd!(DepthEditorOperationCommand.AddEditorDepthOp)(ctx, this, editor, operation, index).succeeded;
    }

    bool commitOperationUpdate(DepthMeshEditorOne editor, ptrdiff_t index, DepthOperation operation) {
        auto ctx = new Context();
        return cmd!(DepthEditorOperationCommand.UpdateEditorDepthOp)(ctx, this, editor, cast(int)index, operation).succeeded;
    }

    bool commitOperationRemove(DepthMeshEditorOne editor, ptrdiff_t index) {
        auto ctx = new Context();
        return cmd!(DepthEditorOperationCommand.RemoveEditorDepthOp)(ctx, this, editor, cast(int)index).succeeded;
    }

public:
    this(bool buildRenderResources = true) {
        this.buildRenderResources = buildRenderResources;
        if (buildRenderResources)
            renderer = new DepthTextureMeshRenderer();
        actionScope = ngOpenActionStackScope(ActionStackScopeUnit.DepthEdit);
    }

    ~this() {
        dispose();
    }

    void dispose() {
        closeStack();
        foreach (editor; editors.byValue) editor.dispose();
        editors.clear();
        operations.clear();
    }

    void closeStack() {
        if (actionScope) {
            actionScope.close();
            actionScope = null;
        }
    }

    void setTargets(Node[] targets) {
        DepthMeshEditorOne[GridDeformer] next;
        foreach (node; targets) {
            if (!editable(node)) continue;
            auto grid = cast(GridDeformer)node;
            if (grid in editors) {
                next[grid] = editors[grid];
            } else {
                auto editor = new DepthMeshEditorOne(grid, buildRenderResources);
                next[grid] = editor;
                loadOperationsFromTarget(editor);
            }
        }
        foreach (grid, editor; editors) {
            if (!(grid in next)) {
                operations.remove(editor);
                if (selectedOperationEditor is editor) {
                    selectedOperationEditor = null;
                    selectedOperationIndex = -1;
                }
                editor.dispose();
            }
        }
        editors = next;
    }

    GridDeformer[] getTargets() {
        return editors.keys();
    }

    DepthMeshEditorOne getEditorFor(GridDeformer grid) {
        if (grid in editors) return editors[grid];
        return null;
    }

    DepthMeshEditorOne[] getEditors() {
        return editors.values();
    }

    void resetFromTargets() {
        foreach (editor; editors.byValue) {
            editor.resetFromTarget();
            replaceOperations(editor, null);
            loadOperationsFromTarget(editor);
        }
    }

    void applyToTargets() {
        incActionPushGroup();
        foreach (editor; editors.byValue) {
            auto ctx = new Context();
            cmd!(DepthMapCommand.SetDepthOps)(ctx, editor.targetNode(), operationsToJson(editor));
            cmd!(DepthMapCommand.ApplyDepthOps)(ctx, editor.targetNode());
            editor.resetFromTarget();
        }
        incActionPopGroup();
    }

    void draw(Camera viewportCamera, ref DepthCamera3D depthCamera) {
        DepthMeshEditorOne hotEditor;
        ptrdiff_t hotIndex;
        DepthOperationHandle hotHandle;
        findOperationHit(-incInputGetMousePosition(), depthCamera, hotEditor, hotIndex, hotHandle);

        foreach (editor; editors.byValue) {
            if (renderer !is null)
                editor.draw(viewportCamera, depthCamera, renderer);
            if (editor in operations) {
                foreach (i, op; operations[editor]) {
                    auto index = cast(ptrdiff_t)i;
                    bool selected = editor is selectedOperationEditor && index == selectedOperationIndex;
                    DepthOperationHandle handle = DepthOperationHandle.None;
                    if (draggingOperation && selected) {
                        handle = selectedOperationHandle;
                    } else if (editor is hotEditor && index == hotIndex) {
                        handle = hotHandle;
                    }
                    op.draw(editor, depthCamera, selected, handle);
                }
            }
        }
    }

    bool update(ImGuiIO* io, Camera viewportCamera) {
        return false;
    }

    DepthMeshEditorOne findNearestProjectedVertex(vec2 point, float radius, out ptrdiff_t vertexIndex) {
        DepthMeshEditorOne bestEditor = null;
        ptrdiff_t bestIndex = -1;
        float bestDistance = radius;
        foreach (editor; editors.byValue) {
            auto index = editor.nearestProjectedVertex(point, bestDistance);
            if (index < 0) continue;
            auto projected = editor.projectedPoints[index];
            auto distance = (projected - point).length();
            if (distance < bestDistance) {
                bestDistance = distance;
                bestEditor = editor;
                bestIndex = index;
            }
        }
        vertexIndex = bestIndex;
        return bestEditor;
    }

    DepthOperation[] copyOperations(DepthMeshEditorOne editor) {
        DepthOperation[] result;
        if (editor in operations) {
            foreach (op; operations[editor]) result ~= op.clone();
        }
        return result;
    }

    ptrdiff_t indexOfOperationInstance(DepthMeshEditorOne editor, DepthOperation operation) {
        if (editor is null || operation is null || !(editor in operations)) return -1;
        foreach (i, op; operations[editor]) {
            if (op is operation) return cast(ptrdiff_t)i;
        }
        return -1;
    }

    void selectOperation(DepthMeshEditorOne editor, ptrdiff_t index) {
        selectedOperationEditor = editor;
        selectedOperationIndex = index;
    }

    void replaceOperations(DepthMeshEditorOne editor, DepthOperation[] nextOperations) {
        if (editor is null) return;
        if (nextOperations.length == 0) {
            operations.remove(editor);
        } else {
            DepthOperation[] copied;
            foreach (op; nextOperations) copied ~= op.clone();
            operations[editor] = copied;
        }
        recompute(editor);
    }

    void appendOperation(DepthMeshEditorOne editor, DepthOperation operation) {
        if (editor is null || operation is null) return;
        operations[editor] ~= operation;
        recompute(editor);
    }

    void recompute(DepthMeshEditorOne editor) {
        if (editor is null) return;
        editor.resetWorkingDepths();
        if (editor in operations) {
            DepthRingOperation[] rings;
            DepthAttachedPointOperation[] attachedPoints;
            DepthPlaneOperation[] planes;
            foreach (op; operations[editor]) {
                if (auto ring = cast(DepthRingOperation)op) {
                    rings ~= ring;
                } else if (auto attached = cast(DepthAttachedPointOperation)op) {
                    attachedPoints ~= attached;
                } else if (auto plane = cast(DepthPlaneOperation)op) {
                    planes ~= plane;
                }
            }

            applyRingNormalSurfaces(editor, rings);
            foreach (op; attachedPoints) op.apply(editor);
            foreach (op; planes) op.apply(editor);
        }
    }

    DepthOperation selectedOperation() {
        if (selectedOperationEditor is null) return null;
        if (!(selectedOperationEditor in operations)) return null;
        auto list = operations[selectedOperationEditor];
        if (selectedOperationIndex < 0 || selectedOperationIndex >= list.length) return null;
        return list[selectedOperationIndex];
    }

    bool findOperationHit(vec2 mouse, ref DepthCamera3D depthCamera, out DepthMeshEditorOne hitEditor, out ptrdiff_t hitIndex, out DepthOperationHandle hitHandle) {
        float bestDistance = float.max;
        hitEditor = null;
        hitIndex = -1;
        hitHandle = DepthOperationHandle.None;
        foreach (editor; editors.byValue) {
            if (!(editor in operations)) continue;
            foreach (i, op; operations[editor]) {
                float distance;
                auto handle = op.hit(editor, mouse, depthCamera, 14.0f / max(0.01f, incViewportZoom), distance);
                if (handle == DepthOperationHandle.None || distance >= bestDistance) continue;
                bestDistance = distance;
                hitEditor = editor;
                hitIndex = cast(ptrdiff_t)i;
                hitHandle = handle;
            }
        }
        return hitEditor !is null;
    }

    bool beginOperationDrag(vec2 mouse, float mouseY, ref DepthCamera3D depthCamera) {
        DepthMeshEditorOne hitEditor;
        ptrdiff_t hitIndex;
        DepthOperationHandle hitHandle;
        if (!findOperationHit(mouse, depthCamera, hitEditor, hitIndex, hitHandle)) return false;

        selectedOperationEditor = hitEditor;
        selectedOperationIndex = hitIndex;
        selectedOperationHandle = hitHandle;
        dragStartOperation = selectedOperation().clone();
        dragStartLocal = hitEditor.displayWorldToLocal(mouse, depthCamera);
        dragStartMouseY = mouseY;
        dragOperationAction = new DepthOperationListChangeAction(this, hitEditor);
        draggingOperation = true;
        return true;
    }

    bool updateOperationDrag(vec2 mouse, float mouseY, ref DepthCamera3D depthCamera, bool snapToGrid) {
        if (!draggingOperation) return false;
        auto op = selectedOperation();
        if (op is null || selectedOperationEditor is null || dragStartOperation is null) return false;
        op.drag(selectedOperationHandle, selectedOperationEditor, dragStartOperation, dragStartLocal, selectedOperationEditor.displayWorldToLocal(mouse, depthCamera), dragStartMouseY, mouseY, snapToGrid);
        recompute(selectedOperationEditor);
        return true;
    }

    bool endOperationDrag() {
        if (!draggingOperation) return false;
        if (dragOperationAction !is null) {
            dragOperationAction.updateNewState();
            incActionPush(dragOperationAction);
        }
        draggingOperation = false;
        dragOperationAction = null;
        dragStartOperation = null;
        selectedOperationHandle = DepthOperationHandle.None;
        return true;
    }

    void duplicateSelectedOperation() {
        auto op = selectedOperation();
        if (op is null) return;
        commitOperationAdd(selectedOperationEditor, op.clone());
    }

    void deleteSelectedOperation() {
        auto op = selectedOperation();
        if (op is null) return;
        commitOperationRemove(selectedOperationEditor, selectedOperationIndex);
    }

    void editSelectedFloat(string id, float* value, float speed, float minValue, float maxValue, string fmt = "%.2f") {
        auto op = selectedOperation();
        if (op is null) return;
        auto action = new DepthOperationListChangeAction(this, selectedOperationEditor);
        if (incDragFloat(id, value, speed, minValue, maxValue, fmt, ImGuiSliderFlags.NoRoundToFormat)) {
            recompute(selectedOperationEditor);
            action.updateNewState();
            incActionPush(action);
        }
    }

    bool drawFloatInput(string id, ref float value, float speed, float minValue, float maxValue, string fmt = "%.2f") {
        return incDragFloat(id, &value, speed, minValue, maxValue, fmt, ImGuiSliderFlags.NoRoundToFormat);
    }

    void pushSelectedOperationEdit(DepthOperationListChangeAction action) {
        auto op = selectedOperation();
        if (op is null) return;
        recompute(selectedOperationEditor);
        action.updateNewState();
        incActionPush(action);
    }

    void drawSelectedOperationEditor() {
        auto op = selectedOperation();
        igText(_("Selected Deformer").toStringz);
        if (op is null) {
            igText(_("No operation selected.").toStringz);
            return;
        }

        igText("%s".format(op.label()).toStringz);
        if (auto attached = cast(DepthAttachedPointOperation)op) {
            editSelectedFloat("Amount", &attached.amount, 0.01f, -2.0f, 2.0f);
        } else if (auto ring = cast(DepthRingOperation)op) {
            editSelectedFloat("Amount", &ring.amount, 0.01f, -2.0f, 2.0f);
            float p0x = ring.p0.x;
            if (drawFloatInput("P0 X", p0x, 1.0f, -float.max, float.max, "%.0f")) {
                auto action = new DepthOperationListChangeAction(this, selectedOperationEditor);
                ring.p0 = vec2(p0x, ring.p0.y);
                pushSelectedOperationEdit(action);
            }
            float p0y = ring.p0.y;
            if (drawFloatInput("P0 Y", p0y, 1.0f, -float.max, float.max, "%.0f")) {
                auto action = new DepthOperationListChangeAction(this, selectedOperationEditor);
                ring.p0 = vec2(ring.p0.x, p0y);
                pushSelectedOperationEdit(action);
            }
            float p1x = ring.p1.x;
            if (drawFloatInput("P1 X", p1x, 1.0f, -float.max, float.max, "%.0f")) {
                auto action = new DepthOperationListChangeAction(this, selectedOperationEditor);
                ring.p1 = vec2(p1x, ring.p1.y);
                pushSelectedOperationEdit(action);
            }
            float p1y = ring.p1.y;
            if (drawFloatInput("P1 Y", p1y, 1.0f, -float.max, float.max, "%.0f")) {
                auto action = new DepthOperationListChangeAction(this, selectedOperationEditor);
                ring.p1 = vec2(ring.p1.x, p1y);
                pushSelectedOperationEdit(action);
            }
            editSelectedFloat("P0 Angle", &ring.p0Angle, 1.0f, -360.0f, 360.0f, "%.0f");
            editSelectedFloat("P1 Angle", &ring.p1Angle, 1.0f, -360.0f, 360.0f, "%.0f");
            editSelectedFloat("Width", &ring.width, 1.0f, 1.0f, 400.0f, "%.0f");
            editSelectedFloat("Falloff", &ring.hardness, 0.05f, 0.1f, 8.0f);
        } else if (auto plane = cast(DepthPlaneOperation)op) {
            editSelectedFloat("Target", &plane.targetDepth, 0.01f, -2.0f, 2.0f);
            float centerX = plane.center.x;
            if (drawFloatInput("Center X", centerX, 1.0f, -float.max, float.max, "%.0f")) {
                auto action = new DepthOperationListChangeAction(this, selectedOperationEditor);
                plane.center = vec2(centerX, plane.center.y);
                pushSelectedOperationEdit(action);
            }
            float centerY = plane.center.y;
            if (drawFloatInput("Center Y", centerY, 1.0f, -float.max, float.max, "%.0f")) {
                auto action = new DepthOperationListChangeAction(this, selectedOperationEditor);
                plane.center = vec2(plane.center.x, centerY);
                pushSelectedOperationEdit(action);
            }
            editSelectedFloat("Radius X", &plane.radiusX, 1.0f, 1.0f, 1000.0f, "%.0f");
            editSelectedFloat("Radius Y", &plane.radiusY, 1.0f, 1.0f, 1000.0f, "%.0f");
            editSelectedFloat("Angle", &plane.angle, 1.0f, -180.0f, 180.0f, "%.0f");
            editSelectedFloat("Flatten", &plane.flattenStrength, 0.01f, 0.0f, 1.0f);
        }

        if (igButton(__("Duplicate"), ImVec2(0, 0))) duplicateSelectedOperation();
        igSameLine();
        if (igButton(__("Delete"), ImVec2(0, 0))) deleteSelectedOperation();
    }

    void drawOperationOptions() {
        igText(_("Depth Deformers").toStringz);
        bool hasOperations = false;
        foreach (editor; editors.byValue) {
            if (!(editor in operations)) continue;
            auto list = operations[editor];
            if (list.length == 0) continue;
            hasOperations = true;
            igText("%s".format(editor.getTarget().name).toStringz);
            foreach (i, op; list) {
                auto selected = editor is selectedOperationEditor && cast(ptrdiff_t)i == selectedOperationIndex;
                auto label = "%s  %s###depth_operation_%s_%s".format(op.label(), op.valueLabel(), cast(void*)editor, i);
                if (igSelectable(label.toStringz, selected, ImGuiSelectableFlags.None, ImVec2(0, 20))) {
                    selectedOperationEditor = editor;
                    selectedOperationIndex = cast(ptrdiff_t)i;
                }
            }
        }
        if (!hasOperations) {
            igText(_("No depth deformers.").toStringz);
        }
        igSeparator();
        drawSelectedOperationEditor();
    }
}
