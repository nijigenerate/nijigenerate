module nijigenerate.viewport.common.mesheditor.tools.bezierdeform;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import i18n;
import nijigenerate.viewport;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
import std.stdio;
import std.math;
import nijigenerate.core.math.vertex;

class BezierDeformTool : NodeSelect {
    uint lastActivePoint;
    uint pathDragTarget;
    uint lockedPoint;

    enum BezierDeformActionID {
        SwitchMode = cast(int)(SelectActionID.End),
        RemovePoint,
        AddPoint,
        TranslatePoint,
        StartTransform,
        StartShiftTransform,
        Transform,
        Rotate,
        SetRotateCenter,
        UnsetRotateCenter,
        Shift
    }

    bool _isShiftMode = false;
    bool _isRotateMode = false;

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        pathDragTarget = -1;
        lockedPoint = -1;
        lastActivePoint = -1;
        super.setToolMode(toolMode, impl);
    }

    bool getIsShiftMode() { return _isShiftMode; }
    void setIsShiftMode(bool value) { _isShiftMode = value; }
    bool getIsRotateMode() { return _isRotateMode; }
    void setIsRotateMode(bool value) { _isRotateMode = value; }

    int peekVertexEdit(ImGuiIO* io, IncMeshEditorOne impl) {
        super.peek(io, impl);
        auto deformImpl = cast(IncMeshEditorOneDeformable)impl;

        if (incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            if (impl.isSelecting)
                impl.adjustPathTransform();
            onDragEnd(impl.mousePos, impl);
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);
        
        if (igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
            int idx = findPoint(deformImpl.vertices, impl.mousePos);
            if (idx != -1) return BezierDeformActionID.RemovePoint;
            else return BezierDeformActionID.AddPoint;

        }

        int action = SelectActionID.None;

        bool preDragging = isDragging;
        if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            if (pathDragTarget != -1)  {
                isDragging = true;
            }
        }

        if (isDragging && pathDragTarget != -1) {
            if (pathDragTarget != lockedPoint) {
                if (lockedPoint != -1) {
                    action = BezierDeformActionID.Rotate;
                } else if (io.KeyShift || _isShiftMode) {
                    if (isDragging != preDragging)
                        action = BezierDeformActionID.StartShiftTransform;
                    else
                        action = BezierDeformActionID.Shift;
                } else {
                    if (isDragging != preDragging)
                        action = BezierDeformActionID.StartTransform;
                    else
                        action = BezierDeformActionID.Transform;
                }
            }
        }

        if (action != SelectActionID.None)
            return action;

        if (pathDragTarget == -1 && io.KeyAlt) {
            // Left click selection
            if (igIsMouseClicked(ImGuiMouseButton.Left)) {
                if (impl.isPointOver(impl.mousePos)) {
                    if (io.KeyShift) return SelectActionID.ToggleSelect;
                    else if (!impl.isSelected(impl.vtxAtMouse))  return SelectActionID.SelectOne;
                    else return SelectActionID.MaybeSelectOne;
                } else {
                    return SelectActionID.SelectArea;
                }
            }
            if (!isDragging && !impl.isSelecting &&
                incInputIsMouseReleased(ImGuiMouseButton.Left) && impl.maybeSelectOne != ulong(-1)) {
                return SelectActionID.SelectMaybeSelectOne;
            }

            // Dragging
            if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
                if (!impl.isSelecting) {
                    return SelectActionID.StartDrag;
                }
            }
        }

        return SelectActionID.None;

    }
    
    int peekDeformEdit(ImGuiIO* io, IncMeshEditorOne impl) {
        super.peek(io, impl);
        auto deformImpl = cast(IncMeshEditorOneDeformable)impl;

        if (incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            if (impl.isSelecting)
                impl.adjustPathTransform();
            onDragEnd(impl.mousePos, impl);
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);
        
        if (!impl.hasAction()) {
            impl.getCleanDeformAction();
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) {
            auto target = findPoint(deformImpl.vertices, impl.mousePos);
            if (target != -1 && (io.KeyCtrl || _isRotateMode)) {
                if (target == lockedPoint)
                    return BezierDeformActionID.UnsetRotateCenter;
                else if (target != -1)
                    return BezierDeformActionID.SetRotateCenter;
            } else {
                pathDragTarget = target;
            }
        }

        int action = SelectActionID.None;

        bool preDragging = isDragging;
        if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            if (pathDragTarget != -1)  {
                isDragging = true;
            }
        }

        if (isDragging && pathDragTarget != -1) {
            if (pathDragTarget != lockedPoint) {
                if (lockedPoint != -1) {
                    action = BezierDeformActionID.Rotate;
                } else if (io.KeyShift || _isShiftMode) {
                    if (isDragging != preDragging)
                        action = BezierDeformActionID.StartShiftTransform;
                    else
                        action = BezierDeformActionID.Shift;
                } else {
                    if (isDragging != preDragging)
                        action = BezierDeformActionID.StartTransform;
                    else
                        action = BezierDeformActionID.Transform;
                }
            }
        }

        if (action != SelectActionID.None)
            return action;

        if (pathDragTarget == -1 && io.KeyAlt) {
            // Left click selection
            if (igIsMouseClicked(ImGuiMouseButton.Left)) {
                if (impl.isPointOver(impl.mousePos)) {
                    if (io.KeyShift) return SelectActionID.ToggleSelect;
                    else if (!impl.isSelected(impl.vtxAtMouse))  return SelectActionID.SelectOne;
                    else return SelectActionID.MaybeSelectOne;
                } else {
                    return SelectActionID.SelectArea;
                }
            }
            if (!isDragging && !impl.isSelecting &&
                incInputIsMouseReleased(ImGuiMouseButton.Left) && impl.maybeSelectOne != ulong(-1)) {
                return SelectActionID.SelectMaybeSelectOne;
            }

            // Dragging
            if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
                if (!impl.isSelecting) {
                    return SelectActionID.StartDrag;
                }
            }
        }

        return SelectActionID.None;

    }

    override 
    int peek(ImGuiIO* io, IncMeshEditorOne impl) {
        int result = super.peek(io, impl);
        if (result != SelectActionID.None) return result;

        if (impl.deformOnly)
            return peekDeformEdit(io, impl);
        else
            return peekVertexEdit(io, impl);
    }

    override
    int unify(int[] actions) {
        int[int] priorities;
        priorities[BezierDeformActionID.SwitchMode] = 2;
        priorities[BezierDeformActionID.RemovePoint] = 1;
        priorities[BezierDeformActionID.AddPoint] = 1;
        priorities[BezierDeformActionID.TranslatePoint] = 0;
        priorities[BezierDeformActionID.StartTransform] = 0;
        priorities[BezierDeformActionID.StartShiftTransform] = 0;
        priorities[BezierDeformActionID.Shift] = 0;
        priorities[BezierDeformActionID.Transform] = 0;
        priorities[BezierDeformActionID.Rotate] = 0;
        priorities[BezierDeformActionID.SetRotateCenter] = 0;
        priorities[BezierDeformActionID.UnsetRotateCenter] = 0;
        priorities[SelectActionID.None]                 = 10;
        priorities[SelectActionID.SelectArea]           = 5;
        priorities[SelectActionID.ToggleSelect]         = 2;
        priorities[SelectActionID.SelectOne]            = 2;
        priorities[SelectActionID.MaybeSelectOne]       = 2;
        priorities[SelectActionID.StartDrag]            = 2;
        priorities[SelectActionID.SelectMaybeSelectOne] = 2;

        int action = SelectActionID.None;
        int curPriority = priorities[action];
        foreach (a; actions) {
            auto newPriority = priorities[a];
            if (newPriority < curPriority) {
                curPriority = newPriority;
                action = a;
            }
        }
        return action;

    }

    bool updateVertexEdit(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        auto deformImpl = cast(IncMeshEditorOneDeformable)impl;

        incStatusTooltip(_("Create/Destroy"), _("Left Mouse (x2)"));
        incStatusTooltip(_("Switch Mode"), _("TAB"));
        incStatusTooltip(_("Toggle locked point"), _("Ctrl"));
        incStatusTooltip(_("Move point along with the path"), _("Shift"));
        
        if (action == BezierDeformActionID.SwitchMode) {
            impl.getCleanDeformAction();
        }

        if (action == BezierDeformActionID.RemovePoint || action == BezierDeformActionID.AddPoint) {
            if (action == BezierDeformActionID.RemovePoint) {
                int idx = findPoint(deformImpl.vertices, impl.mousePos);
                if(idx != -1) deformImpl.removeVertex(io, true);
            } else if (action == BezierDeformActionID.AddPoint) {
                deformImpl.addVertex(io);
            }
            pathDragTarget = -1;
            lockedPoint    = -1;
        } else if (action == BezierDeformActionID.UnsetRotateCenter) {
            lockedPoint = -1;
            pathDragTarget = -1;
            _isRotateMode = false;

        } else if (action == BezierDeformActionID.SetRotateCenter) {
            auto target = findPoint(deformImpl.vertices, impl.mousePos);
            lockedPoint = target;
            pathDragTarget = -1;
            _isRotateMode = false;

        } else if (action == BezierDeformActionID.Rotate) {
            int step = (pathDragTarget > lockedPoint)? 1: -1;
            vec2 prevRelPosition = impl.lastMousePos - deformImpl.vertices[lockedPoint].position;
            vec2 relPosition     = impl.mousePos - deformImpl.vertices[lockedPoint].position;
            float prevAngle = atan2(prevRelPosition.y, prevRelPosition.x);
            float angle     = atan2(relPosition.y, relPosition.x);
            float relAngle = angle - prevAngle;
            mat4 rotate = mat4.identity.translate(vec3(-deformImpl.vertices[lockedPoint].position, 0)).rotateZ(relAngle).translate(vec3(deformImpl.vertices[lockedPoint].position, 0));

            for (int i = lockedPoint + step; 0 <= i && i < deformImpl.vertices.length; i += step) {
                deformImpl.vertices[i].position = (rotate * vec4(deformImpl.vertices[i].position, 0, 1)).xy;
            }

        } else if (action == BezierDeformActionID.Shift || action == BezierDeformActionID.StartShiftTransform) {
  
            if(pathDragTarget != -1){
//                float off = findClosestPointOffset(deformImpl.vertices, impl.mousePos);
//                vec2 pos  = path.eval(off);
                vec2 pos;
                deformImpl.vertices[pathDragTarget].position = pos;
            }
        
        } else if (action == BezierDeformActionID.Transform || action == BezierDeformActionID.StartTransform) {
            if(pathDragTarget != -1){
                vec2 relTranslation = impl.mousePos - impl.lastMousePos;
                deformImpl.vertices[pathDragTarget].position += relTranslation;
            }
        }

        // Left click selection
        if (action == SelectActionID.ToggleSelect) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.toggleSelect(impl.vtxAtMouse);
        } else if (action == SelectActionID.SelectOne) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.selectOne(impl.vtxAtMouse);
            else
                impl.deselectAll();
        } else if (action == SelectActionID.MaybeSelectOne) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.maybeSelectOne = impl.vtxAtMouse;
        } else if (action == SelectActionID.SelectArea) {
            impl.selectOrigin = impl.mousePos;
            impl.isSelecting = true;
        }

        if (action == SelectActionID.SelectMaybeSelectOne) {
            if (impl.maybeSelectOne != ulong(-1))
                impl.selectOne(impl.maybeSelectOne);
        }

        // Dragging
        if (action == SelectActionID.StartDrag) {
            onDragStart(impl.mousePos, impl);
        }

        if (changed) impl.refreshMesh();
        return changed;
    }

    bool updateDeformEdit(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        auto deformImpl = cast(IncMeshEditorOneDeformable)impl;

        incStatusTooltip(_("Deform"), _("Left Mouse"));
        incStatusTooltip(_("Switch Mode"), _("TAB"));
        incStatusTooltip(_("Toggle locked point"), _("Ctrl"));
        incStatusTooltip(_("Move point along with the path"), _("Shift"));

        if (!impl.hasAction())
            impl.getCleanDeformAction();

        if (action == BezierDeformActionID.StartTransform || action == BezierDeformActionID.StartShiftTransform) {
            auto deform = (cast(MeshEditorAction!DeformationAction)(impl.getDeformAction()));
            if(deform !is null) deform.clear();
        }

        if (action == BezierDeformActionID.UnsetRotateCenter) {
            lockedPoint = -1;
            pathDragTarget = -1;
            _isRotateMode = false;

        } else if (action == BezierDeformActionID.SetRotateCenter) {
            auto target = findPoint(deformImpl.vertices, impl.mousePos);
            lockedPoint = target;
            pathDragTarget = -1;
            _isRotateMode = false;

        } else if (action == BezierDeformActionID.Rotate) {
            int step = (pathDragTarget > lockedPoint)? 1: -1;
            vec2 prevRelPosition = impl.lastMousePos - deformImpl.vertices[lockedPoint].position;
            vec2 relPosition     = impl.mousePos - deformImpl.vertices[lockedPoint].position;
            float prevAngle = atan2(prevRelPosition.y, prevRelPosition.x);
            float angle     = atan2(relPosition.y, relPosition.x);
            float relAngle = angle - prevAngle;
            mat4 rotate = mat4.identity.translate(vec3(-deformImpl.vertices[lockedPoint].position, 0)).rotateZ(relAngle).translate(vec3(deformImpl.vertices[lockedPoint].position, 0));

            for (int i = lockedPoint + step; 0 <= i && i < deformImpl.vertices.length; i += step) {
                deformImpl.vertices[i].position = (rotate * vec4(deformImpl.vertices[i].position, 0, 1)).xy;
            }

        } else if (action == BezierDeformActionID.Shift || action == BezierDeformActionID.StartShiftTransform) {
            if(pathDragTarget != -1){
//                float off = path.findClosestPointOffset(impl.mousePos);
//                vec2 pos  = path.eval(off);
                vec2 pos;
                deformImpl.vertices[pathDragTarget].position = pos;
            }
        
        } else if (action == BezierDeformActionID.Transform || action == BezierDeformActionID.StartTransform) {
            if(pathDragTarget != -1){
                vec2 relTranslation = impl.mousePos - impl.lastMousePos;
                deformImpl.vertices[pathDragTarget].position += relTranslation;
            }
        }

        mat4 trans = impl.updatePathTarget();
        if (impl.hasAction())
            impl.markActionDirty();
        changed = true;

        // Left click selection
        if (action == SelectActionID.ToggleSelect) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.toggleSelect(impl.vtxAtMouse);
        } else if (action == SelectActionID.SelectOne) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.selectOne(impl.vtxAtMouse);
            else
                impl.deselectAll();
        } else if (action == SelectActionID.MaybeSelectOne) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.maybeSelectOne = impl.vtxAtMouse;
        } else if (action == SelectActionID.SelectArea) {
            impl.selectOrigin = impl.mousePos;
            impl.isSelecting = true;
        }

        if (action == SelectActionID.SelectMaybeSelectOne) {
            if (impl.maybeSelectOne != ulong(-1))
                impl.selectOne(impl.maybeSelectOne);
        }

        // Dragging
        if (action == SelectActionID.StartDrag) {
            onDragStart(impl.mousePos, impl);
        }

        if (changed) impl.refreshMesh();
        return changed;
    }

    override 
    bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        super.update(io, impl, action, changed);
        if (impl.deformOnly)
            updateDeformEdit(io, impl, action, changed);
        else
            updateVertexEdit(io, impl, action, changed);
        return changed;
    }

    override
    void draw(Camera camera, IncMeshEditorOne impl) {
        super.draw(camera, impl);
/*
        if (path && path.target && impl.deforming) {
            path.draw(impl.transform, vec4(0, 0.6, 0.6, 1), lockedPoint);
            path.target.draw(impl.transform, vec4(0, 1, 0, 1), lockedPoint);
        } else if (path) {
            if (path.target) path.target.draw(impl.transform, vec4(0, 0.6, 0, 1), lockedPoint);
            path.draw(impl.transform, vec4(0, 1, 1, 1), lockedPoint);
        }
        */
    }

    override
    MeshEditorAction!DeformationAction editorAction(Node target, DeformationAction action) {
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    override
    MeshEditorAction!GroupAction editorAction(Node target, GroupAction action) {
        return new MeshEditorAction!(GroupAction)(target, action);
    }

    override
    MeshEditorAction!DeformationAction editorAction(Drawable target, DeformationAction action) {
        return new MeshEditorAction!(DeformationAction)(target, action);
    }

    override
    MeshEditorAction!GroupAction editorAction(Drawable target, GroupAction action) {
        return new MeshEditorAction!(GroupAction)(target, action);
    }
}

class ToolInfoImpl(T: BezierDeformTool) : ToolInfoBase!(T) {
    override
    void setupToolMode(IncMeshEditorOne e, VertexToolMode mode) {
        e.setToolMode(mode);
        e.deforming = false;
        e.refreshMesh();
    }

    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (deformOnly) {
            return super.viewportTools(deformOnly, toolMode, editors);
        }
        return false;
    }
    
    override
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) { 
        igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
        igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(4, 4));
        auto deformTool = cast(PathDeformTool)(editors.length == 0 ? null: editors.values()[0].getTool());
        igBeginGroup();
            if (incButtonColored("", ImVec2(0, 0), (deformTool !is null && deformTool.getIsRotateMode()) ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) { // rotation mode
                foreach (e; editors) {
                    auto deform = cast(PathDeformTool)(e.getTool());
                    if (deform !is null)
                        deform.setIsRotateMode(!deform.getIsRotateMode());
                }
            }
            incTooltip(_("Set rotation center"));
        igEndGroup();

        igSameLine(0, 4);

        igBeginGroup();
            if (incButtonColored("", ImVec2(0, 0), (deformTool !is null && deformTool.getIsShiftMode()) ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) { // move shift
                foreach (e; editors) {
                    auto deform = cast(PathDeformTool)(e.getTool());
                    if (deform !is null)
                        deform.setIsShiftMode(!deform.getIsShiftMode());
                }
            }
            incTooltip(_("Move points along the path"));
        igEndGroup();
        igPopStyleVar(2);
        return false;
    }
    override VertexToolMode mode() { return VertexToolMode.PathDeform; }
    override string icon() { return "";}
    override string description() { return _("Path Deform Tool");}
}