module nijigenerate.viewport.common.mesheditor.tools.point;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import i18n;
import nijigenerate.viewport.base;
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
//import std.stdio;

class PointTool : NodeSelect {
    Action action;

    // using static for remembering the last setting
    static bool autoConnect = false;

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

    enum PointActionID {
        Add = cast(int)(SelectActionID.End),
        Remove,
        Translate,
        TranslateUp,
        TranslateDown,
        TranslateLeft,
        TranslateRight,
        End
    }


    bool updateMeshEdit(ImGuiIO* io, IncMeshEditorOne impl, out bool changed) {
        incStatusTooltip(_("Select"), _("Left Mouse"));
        incStatusTooltip(_("Create"), _("Ctrl+Left Mouse"));

        if (incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);

        auto implDrawable = cast(IncMeshEditorOneDrawable)impl;
        auto implDeformable = cast(IncMeshEditorOneDeformable)impl;
        assert(implDrawable !is null || implDeformable !is null);
        
        void addOrRemoveVertex(T)(T implD, bool selectedOnly) {
            // Check if mouse is over a vertex
            auto vtxAtMouse = impl.getVerticesByIndex([impl.vtxAtMouse])[0];
            if (vtxAtMouse !is null) {
                auto action = new VertexRemoveAction(impl.getTarget().name, impl);

                if (!selectedOnly || impl.isSelected(impl.vtxAtMouse)) {
                    MeshVertex*[] removingVertices;
                    impl.foreachMirror((uint axis) {
                        ulong index = impl.getVertexFromPoint(impl.mirror(axis, impl.mousePos));
                        MeshVertex* vertex = impl.getVerticesByIndex([index])[0];
                        if (vertex !is null)
                            removingVertices ~= vertex;
                    });
                    foreach (vertex; removingVertices)
                        action.removeVertex(vertex);
                    impl.refreshMesh();
                    impl.vertexMapDirty = true;
                    impl.selected.length = 0;
                    impl.updateMirrorSelected();
                    impl.maybeSelectOne = ulong(-1);
                    impl.vtxAtMouse = ulong(-1);
                    changed = true;
                }

                action.updateNewState();
                incActionPush(action);
            } else {
                auto action = new VertexAddAction(impl.getTarget().name, impl);

                ulong off = implD.vertices.length;
                if (impl.isOnMirror(impl.mousePos, impl.meshEditAOE)) {
                    impl.placeOnMirror(impl.mousePos, impl.meshEditAOE);
                } else {
                    impl.foreachMirror((uint axis) {
                        MeshVertex* vertex = new MeshVertex(impl.mirror(axis, impl.mousePos));
                        action.addVertex(vertex);
                    });
                }
                impl.refreshMesh();
                impl.vertexMapDirty = true;
                if (io.KeyCtrl) impl.selectOne(implD.vertices.length - 1);
                else impl.selectOne(off);
                action.updateNewState();
                incActionPush(action);
                changed = true;
                /*
                /// FIXME: Disabled auto connection because Action class is changed and degraded in bezier-deform branch.

                // connect if there is a selected vertex
                void connectVertex(ref MeshVertex* vertex) {
                    if (vertex is null) return;

                    // search last MeshAddAction to connect
                    auto lastAddAction = incActionFindLast!VertexAddAction(3);
                    if (lastAddAction is null || lastAddAction.vertices.length == 0) return;

                    auto prevVertexIdx = impl.getVertexFromPoint(lastAddAction.axisVertices[0][$ - 1].position);
                    auto prevVertex = impl.getVerticesByIndex([prevVertexIdx])[0];
                    if (prevVertex == null) return;
                    auto action = new MeshConnectAction(impl.getTarget().name, impl, mesh);
                    impl.foreachMirror((uint axis) {
                        MeshVertex* mPrev = impl.mirrorVertex(axis, prevVertex);
                        MeshVertex* mSel  = impl.mirrorVertex(axis, vertex);

                        if (mPrev !is null && mSel !is null) {
                            action.connect(mPrev, mSel);
                        }
                    });
                    impl.refreshMesh();
                    action.updateNewState();
                    incActionPush(action);

                    changed = true;
                }

                MeshVertex* vertex;
                addVertex(vertex);
                */
//                if (autoConnect)
//                    connectVertex(vertex);
            }
        }

        // Key actions
        if (incInputIsKeyPressed(ImGuiKey.Delete)) {
            auto action = new VertexRemoveAction(impl.getTarget().name, impl);

            impl.foreachMirror((uint axis) {
                foreach(v; impl.selected) {
                    ulong vInd2 = impl.mirrorVertex(axis, v);
                    if (vInd2 != ulong(-1)) {
                        MeshVertex* v2 = impl.getVerticesByIndex([vInd2])[0];
                        action.removeVertex(v2, false);
                    }
                }
            });
            action.removeVertices();
            action.updateNewState();
            incActionPush(action);

            impl.selected = [];
            impl.updateMirrorSelected();
            impl.refreshMesh();
            impl.vertexMapDirty = true;
            changed = true;
        }

        VertexMoveAction moveAction = null;
        void shiftSelection(vec2 delta) {
            float magnitude = 10.0;
            if (io.KeyAlt) magnitude = 1.0;
            else if (io.KeyShift) magnitude = 100.0;
            delta *= magnitude;

            impl.foreachMirror((uint axis) {
                vec2 mDelta = impl.mirrorDelta(axis, delta);
                foreach(v; impl.selected) {
                    ulong vInd2 = impl.mirrorVertex(axis, v);
                    auto v2 = impl.getVerticesByIndex([vInd2])[0];
                    if (v2 !is null) {
                        if (moveAction is null) {
                            moveAction = new VertexMoveAction(impl.getTarget().name, impl);
                        }
                        moveAction.moveVertex(v2, v2.position + mDelta);
                    }
                }
            });
            impl.refreshMesh();
            changed = true;
        }

        if (incInputIsKeyPressed(ImGuiKey.LeftArrow)) {
            shiftSelection(vec2(-1, 0));
        } else if (incInputIsKeyPressed(ImGuiKey.RightArrow)) {
            shiftSelection(vec2(1, 0));
        } else if (incInputIsKeyPressed(ImGuiKey.DownArrow)) {
            shiftSelection(vec2(0, 1));
        } else if (incInputIsKeyPressed(ImGuiKey.UpArrow)) {
            shiftSelection(vec2(0, -1));
        }

        if (moveAction !is null) {
            moveAction.updateNewState();
            incActionPush(moveAction);
        }

        // Left click selection
        if (igIsMouseClicked(ImGuiMouseButton.Left)) {
            if (io.KeyCtrl && !io.KeyShift) {
                // Add/remove action
                if (implDrawable)
                    addOrRemoveVertex(implDrawable, false);
                else
                    addOrRemoveVertex(implDeformable, false);
            } else {
                // Select / drag start
                if (impl.isPointOver(impl.mousePos)) {
                    if (io.KeyShift) impl.toggleSelect(impl.vtxAtMouse);
                    else if (!impl.isSelected(impl.vtxAtMouse))  impl.selectOne(impl.vtxAtMouse);
                    else impl.maybeSelectOne = impl.vtxAtMouse;
                } else {
                    impl.selectOrigin = impl.mousePos;
                    impl.isSelecting = true;
                }
            }
        }
        if (!isDragging && !impl.isSelecting &&
            incInputIsMouseReleased(ImGuiMouseButton.Left) && impl.maybeSelectOne != ulong(-1)) {
            impl.selectOne(impl.maybeSelectOne);
        }

        // Left double click action
        if (igIsMouseDoubleClicked(ImGuiMouseButton.Left) && !io.KeyShift && !io.KeyCtrl) {
            if (implDrawable)
                addOrRemoveVertex(implDrawable, true);
            else
                addOrRemoveVertex(implDeformable, true);
        }

        // Dragging
        if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            onDragStart(impl.mousePos, impl);
        }

        onDragUpdate(impl.mousePos, impl);
        return true;
    }


    int peekDeformEdit(ImGuiIO* io, IncMeshEditorOne impl) {

        if (incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);

        if (incInputIsKeyPressed(ImGuiKey.LeftArrow)) {
            return PointActionID.TranslateLeft;
        } else if (incInputIsKeyPressed(ImGuiKey.RightArrow)) {
            return PointActionID.TranslateRight;
        } else if (incInputIsKeyPressed(ImGuiKey.DownArrow)) {
            return PointActionID.TranslateDown;
        } else if (incInputIsKeyPressed(ImGuiKey.UpArrow)) {
            return PointActionID.TranslateUp;
        }

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


        if (isDragging) {
            return PointActionID.Translate;
        }

        // Dragging
        if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            if (!impl.isSelecting) {
                return SelectActionID.StartDrag;
            }
        }

        return SelectActionID.None;
    }

    bool updateDeformEdit(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {

        incStatusTooltip(_("Select"), _("Left Mouse"));

        bool keyboardMoved = false;
        void shiftSelection(vec2 delta) {
            float magnitude = 10.0;
            if (io.KeyAlt) magnitude = 1.0;
            else if (io.KeyShift) magnitude = 100.0;
            delta *= magnitude;

            impl.foreachMirror((uint axis) {
                vec2 mDelta = impl.mirrorDelta(axis, delta);
                foreach(vInd; impl.selected) {
                    MeshVertex*[] vs = impl.getVerticesByIndex([vInd, impl.mirrorVertex(axis, vInd)]);
                    MeshVertex* v  = vs[0];
                    MeshVertex* v2 = vs[1];
                    impl.getDeformAction();
                    impl.updateAddVertexAction(v);
                    impl.markActionDirty();
                    keyboardMoved = true;
                    if (v2 !is null) v2.position += mDelta;
                }
            });
            impl.refreshMesh();
            changed = true;
        }

        if (action == PointActionID.TranslateLeft) {
            shiftSelection(vec2(-1, 0));
        } else if (action == PointActionID.TranslateRight) {
            shiftSelection(vec2(1, 0));
        } else if (action == PointActionID.TranslateDown) {
            shiftSelection(vec2(0, 1));
        } else if (action == PointActionID.TranslateUp) {
            shiftSelection(vec2(0, -1));
        }
        if (keyboardMoved)
            impl.pushDeformAction();

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

        if (action == PointActionID.Translate)
            changed = onDragUpdate(impl.mousePos, impl) || changed;
        return true;
    }

    override int peek(ImGuiIO* io, IncMeshEditorOne impl) {
        super.peek(io, impl);
        if (impl.deformOnly)
            return peekDeformEdit(io, impl);
        else
            return 0;
    }

    override int unify(int[] actions) {
        int[int] priorities;
        priorities[PointActionID.Add] = 2;
        priorities[PointActionID.Remove] = 2;
        priorities[PointActionID.Translate] = 1;
        priorities[PointActionID.TranslateUp] = 0;
        priorities[PointActionID.TranslateDown] = 0;
        priorities[PointActionID.TranslateLeft] = 0;
        priorities[PointActionID.TranslateRight] = 0;
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

    override bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        super.update(io, impl, action, changed);
        if (impl.deformOnly)
            updateDeformEdit(io, impl, action, changed);
        else
            updateMeshEdit(io, impl, changed);
        return changed;
    }

    bool isAutoConnect() {
        return autoConnect;
    }

    void setAutoConnect(bool autoConnect) {
        this.autoConnect = autoConnect;
    }
}

class ToolInfoImpl(T: PointTool) : ToolInfoBase!(T) {
    override
    VertexToolMode mode() { return VertexToolMode.Points; }
    override
    string icon() { return ""; }
    override
    string description() { return _("Vertex Tool"); }

    override
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) { 
        if (deformOnly) {

        } else {
            auto pointTool = cast(PointTool)(editors.length == 0 ? null: editors.values()[0].getTool());
            igBeginGroup();
                if (incButtonColored("", ImVec2(0, 0), (pointTool !is null && !pointTool.isAutoConnect())? ImVec4(0.6, 0.6, 0.6, 1) : colorUndefined)) {
                foreach (e; editors) {
                    auto pt = cast(PointTool)(e.getTool());
                    if (pt !is null)
                        pt.setAutoConnect(!pt.isAutoConnect());
                }
                }
                incTooltip(_("Auto connect vertices"));
            igEndGroup();
        }

        return false;
    }
}