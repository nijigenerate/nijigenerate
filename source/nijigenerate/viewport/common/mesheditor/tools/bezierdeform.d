module nijigenerate.viewport.common.mesheditor.tools.bezierdeform;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import i18n;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.core.math.vertex;
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
import std.algorithm;
import std.array;
import std.typecons;

class BezierDeformTool : NodeSelect {
    Action action;

    ulong lockedPoint = ulong(-1);

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
        Shift,
        End
    }

    bool _isShiftMode = false;
    bool _isRotateMode = false;
    bool preDragging = false;

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        lockedPoint = ulong(-1);
        _isRotateMode = false;
        _isShiftMode = false;
        super.setToolMode(toolMode, impl);
    }

    bool getIsShiftMode() { return _isShiftMode; }
    void setIsShiftMode(bool value) { _isShiftMode = value; }
    bool getIsRotateMode() { return _isRotateMode; }
    void setIsRotateMode(bool value) { _isRotateMode = value; }

    Nullable!Curve origCurve;
    vec2u origCurvePoint;

    override bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
        if (!impl.deformOnly) {
            if (!impl.isSelecting && !isDragging) {
                isDragging = true;
                action = new VertexMoveAction(impl.getTarget().name, impl);
                return true;
            }
            return false;
        } else {
            return super.onDragStart(mousePos, impl);
        }
    }

    override bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl) {
        if (!impl.deformOnly) {
            if (action !is null) {
                if (auto meshAction = cast(MeshAction)(action)) {
                    if (meshAction.dirty) {
                        meshAction.updateNewState();
                        incActionPush(action);
                    }
                }else if (auto vertAction = cast(VertexAction)(action)) {
                    if (vertAction.dirty) {
                        vertAction.updateNewState();
                        incActionPush(action);
                    }
                }
                action = null;
            }
        }
        preDragging = false;
        return super.onDragEnd(mousePos, impl);
    }

    override bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl) {
        if (!impl.deformOnly) { 
            if (isDragging) {
                if (auto meshAction = cast(VertexMoveAction)action) {
                    foreach(select; impl.selected) {
                        impl.foreachMirror((uint axis) {
                            MeshVertex *v = impl.getVerticesByIndex([impl.mirrorVertex(axis, select)])[0];
                            if (v is null) return;
                            meshAction.moveVertex(v, v.position + impl.mirror(axis, mousePos - impl.lastMousePos));
                        });
                    }
                }

                if (impl.selected.length > 0)
                    impl.maybeSelectOne = ulong(-1);
                impl.refreshMesh();
                return true;
            }
            return false;
        } else {
            return super.onDragUpdate(mousePos, impl);
        }
    }

    int peekVertexEdit(ImGuiIO* io, IncMeshEditorOne impl) {
        auto deformImpl = cast(IncMeshEditorOneDeformable)impl;

        if (incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);
        
        if (igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
            ulong idx = cast(ulong)findPoint(deformImpl.vertices, impl.mousePos, incViewportZoom);
            if (idx != ulong(-1)) return BezierDeformActionID.RemovePoint;
            else return BezierDeformActionID.AddPoint;

        }

        int action = SelectActionID.None;

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

        if (action != SelectActionID.None)
            return action;

        if (isDragging) {
            return BezierDeformActionID.TranslatePoint;
        }

        // Dragging
        if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            if (!impl.isSelecting) {
                return SelectActionID.StartDrag;
            }
        }

        return SelectActionID.None;

    }
    
    int peekDeformEdit(ImGuiIO* io, IncMeshEditorOne impl) {
        auto deformImpl = cast(IncMeshEditorOneDeformable)impl;

        if (incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);
        
        if (!impl.hasAction()) {
            impl.getCleanDeformAction();
        }

        int action = SelectActionID.None;

        if (igIsMouseClicked(ImGuiMouseButton.Left)) {
            auto target = cast(ulong)findPoint(deformImpl.vertices, impl.mousePos, incViewportZoom);

            if (target != ulong(-1)) {
                if (io.KeyCtrl || _isRotateMode) {
                    if (target == lockedPoint)
                        return BezierDeformActionID.UnsetRotateCenter;
                    else if (target != ulong(-1)) {
                        return BezierDeformActionID.SetRotateCenter;
                    }
                } else if (!impl.isSelected(impl.vtxAtMouse)) {
                    return SelectActionID.SelectOne;
                } else { return SelectActionID.MaybeSelectOne; }
            } else {
                return SelectActionID.SelectArea;
            }

        }

        if (!isDragging && !impl.isSelecting &&
            incInputIsMouseReleased(ImGuiMouseButton.Left) && impl.maybeSelectOne != ulong(-1)) {
            return SelectActionID.SelectMaybeSelectOne;
        }

        if (action != SelectActionID.None)
            return action;

        if (isDragging && impl.selected.length > 0) {
            if (impl.selected.length == 1 && impl.selected[0] != lockedPoint) {
                if (lockedPoint != ulong(-1)) {
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
                        action = BezierDeformActionID.TranslatePoint;
                }
            }
            preDragging = true;
        }

        if (action != SelectActionID.None)
            return action;

        // Dragging
        if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            if (!impl.isSelecting) {
                action = SelectActionID.StartDrag;
            }
        }

        return action;

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
        if (deformImpl is null) return false;

        incStatusTooltip(_("Create/Destroy"), _("Left Mouse (x2)"));
        incStatusTooltip(_("Switch Mode"), _("TAB"));
        incStatusTooltip(_("Toggle locked point"), _("Ctrl"));
        incStatusTooltip(_("Move point along with the path"), _("Shift"));
        
        if (action == BezierDeformActionID.SwitchMode) {
            impl.getCleanDeformAction();
        }

        if (action == BezierDeformActionID.RemovePoint || action == BezierDeformActionID.AddPoint) {
            if (action == BezierDeformActionID.RemovePoint) {
                ulong idx = cast(ulong)findPoint(deformImpl.vertices, impl.mousePos, incViewportZoom);

                auto removeAction = new VertexRemoveAction(impl.getTarget().name, impl);
                if (idx != ulong(-1)) {
                    removeAction.removeVertex(impl.getVerticesByIndex([idx])[0]);
                }
                removeAction.updateNewState();
                incActionPush(removeAction);
            } else if (action == BezierDeformActionID.AddPoint) {

                auto insertAction = new VertexInsertAction(impl.getTarget().name, impl);
                if (auto path = cast(PathDeformer)impl.getTarget()) {
                    auto curve = path.createCurve(deformImpl.vertices.map!(v=>v.position).array);
                    auto relVertices = deformImpl.vertices.map!(v=>curve.closestPoint(v.position)).array;
                    float relNew = curve.closestPoint(impl.mousePos);
                    vec2 newPos = curve.point(relNew);
                    bool inserted = false;
                    if (isOverlapped(newPos, impl.mousePos, incViewportZoom)) {
                        foreach (i, rv; relVertices) {
                            if (relNew <= rv) {
                                MeshVertex* vertex = new MeshVertex(newPos);
                                insertAction.insertVertex(cast(int)i, vertex);
                                inserted = true;
                                break;
                            }
                        }
                    } else if (deformImpl.vertices.length > 1 && relNew < 0.5) {
                        MeshVertex* vertex = new MeshVertex(impl.mousePos);
                        insertAction.insertVertex(0, vertex);
                        inserted = true;
                    }
                    if (!inserted) {
                        MeshVertex* vertex = new MeshVertex(impl.mousePos);
                        insertAction.addVertex(vertex);
                    }
                } else {
                    MeshVertex* vertex = new MeshVertex(impl.mousePos);
                    insertAction.addVertex(vertex);
                }

                incActionPush(insertAction);
            }
            impl.deselectAll();
            lockedPoint    = ulong(-1);
        } else if (action == BezierDeformActionID.TranslatePoint || action == BezierDeformActionID.StartTransform) {
            foreach (i; impl.selected) {
                vec2 relTranslation = impl.mousePos - impl.lastMousePos;
                deformImpl.vertices[i].position += relTranslation;
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
        ulong pathDragTarget = impl.selected.length == 1 ? impl.selected[0] : ulong(-1);

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
            lockedPoint = ulong(-1);
            impl.deselectAll();
            _isRotateMode = false;

        } else if (action == BezierDeformActionID.SetRotateCenter) {
            auto target = cast(ulong)findPoint(deformImpl.vertices, impl.mousePos, incViewportZoom);
            lockedPoint = target;
            impl.deselectAll();
            _isRotateMode = false;

        } else if (action == BezierDeformActionID.Rotate) {
            int step = (pathDragTarget > lockedPoint)? 1: -1;
            vec2 prevRelPosition = impl.lastMousePos - deformImpl.vertices[lockedPoint].position;
            vec2 relPosition     = impl.mousePos - deformImpl.vertices[lockedPoint].position;
            float prevAngle = atan2(prevRelPosition.y, prevRelPosition.x);
            float angle     = atan2(relPosition.y, relPosition.x);
            float relAngle = angle - prevAngle;
            mat4 rotate = mat4.identity.translate(vec3(-deformImpl.vertices[lockedPoint].position, 0)).rotateZ(relAngle).translate(vec3(deformImpl.vertices[lockedPoint].position, 0));

            for (int i = cast(int)lockedPoint + step; 0 <= i && i < deformImpl.vertices.length; i += step) {
                deformImpl.vertices[i].position = (rotate * vec4(deformImpl.vertices[i].position, 0, 1)).xy;
            }

        } else if (action == BezierDeformActionID.Shift || action == BezierDeformActionID.StartShiftTransform) {
            if(impl.selected.length == 1){
                if (auto path = cast(PathDeformer)impl.getTarget()) {
                    if (origCurve.isNull || origCurvePoint != incArmedParameter().findClosestKeypoint()) {
                        origCurvePoint = incArmedParameter().findClosestKeypoint();
                        origCurve = path.createCurve(deformImpl.vertices.map!(i=>i.position).array);
                    }
                    vec2 pos = origCurve.get.point(origCurve.get.closestPoint(impl.mousePos));
                    deformImpl.vertices[impl.selected[0]].position = pos;
                }
            }
        
        } else if (action == BezierDeformActionID.TranslatePoint || action == BezierDeformActionID.StartTransform) {
            if(impl.selected.length == 1){
                vec2 relTranslation = impl.mousePos - impl.lastMousePos;
                deformImpl.vertices[impl.selected[0]].position += relTranslation;
            }
        }

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
        auto deformImpl = cast(IncMeshEditorOneDeformable)impl;
        super.draw(camera, impl);
        if (lockedPoint != ulong(-1)) {
            vec3[] drawPoints = [vec3(deformImpl.vertices[lockedPoint].position, 0)];
            inDbgSetBuffer(drawPoints);
            inDbgPointsSize(4);
            inDbgDrawPoints(vec4(0, 1, 0, 1), impl.transform);
        }
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
        bool isDeformer = editors.keys.all!((k) => cast(PathDeformer)k !is null );
        if (isDeformer) {
            return super.viewportTools(deformOnly, toolMode, editors);
        }
        return false;
    }
    
    override
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) { 
        igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
        igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(4, 4));
        auto deformTool = cast(T)(editors.length == 0 ? null: editors.values()[0].getTool());
        igBeginGroup();
            if (incButtonColored("", ImVec2(0, 0), (deformTool !is null && deformTool.getIsRotateMode()) ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) { // rotation mode
                foreach (e; editors) {
                    auto deform = cast(T)(e.getTool());
                    if (deform !is null)
                        deform.setIsRotateMode(!deform.getIsRotateMode());
                }
            }
            incTooltip(_("Set rotation center"));
        igEndGroup();

        igSameLine(0, 4);

        igBeginGroup();
            if (incButtonColored("\ue8e4", ImVec2(0, 0), (deformTool !is null && deformTool.getIsShiftMode()) ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) { // move shift
                foreach (e; editors) {
                    auto deform = cast(T)(e.getTool());
                    if (deform !is null)
                        deform.setIsShiftMode(!deform.getIsShiftMode());
                }
            }
            incTooltip(_("Move points along the path"));
        igEndGroup();
        igPopStyleVar(2);
        return false;
    }
    override VertexToolMode mode() { return VertexToolMode.BezierDeform; }
    override string icon() { return "";}
    override string description() { return _("Path Deform Tool");}
}