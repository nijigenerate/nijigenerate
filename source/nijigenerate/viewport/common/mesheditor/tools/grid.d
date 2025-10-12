module nijigenerate.viewport.common.mesheditor.tools.grid;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import i18n;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.dbg;
import bindbc.opengl;
import bindbc.imgui;
//import std.stdio;
import std.array;
import std.algorithm.searching: countUntil;
import std.algorithm.mutation;
import std.algorithm.sorting;
import std.algorithm.searching : all;
import std.algorithm.iteration : uniq;

class GridTool : NodeSelect {
    GridActionID currentAction;
    int numCut = 3;
    vec2 dragOrigin;
    vec2 dragEnd;
    int dragTargetXIndex = 0;
    int dragTargetYIndex = 0;

    enum GridActionID {
        Add = cast(int)(SelectActionID.End),
        Remove,
        Create,
        TranslateFree,
        TranslateX,
        TranslateY,
        TranslateUp,
        TranslateDown,
        TranslateLeft,
        TranslateRight,
        End
    }

    static float selectRadius = 16f;

    float[][] deriveAxes(IncMeshEditorOneDeformable impl) {
        float[] xs;
        float[] ys;
        foreach (v; impl.vertices) {
            if (v is null) continue;
            xs ~= v.position.x;
            ys ~= v.position.y;
        }
        float[][] axes;
        if (xs.length == 0 || ys.length == 0) return axes;
        xs.sort;
        ys.sort;
        axes.length = 2;
        axes[0] = ys.uniq.array;
        axes[1] = xs.uniq.array;
        return axes;
    }

    float[][] currentAxes(IncMeshEditorOne impl) {
        if (auto drawable = cast(IncMeshEditorOneDrawable)impl) {
            auto mesh = drawable.getMesh();
            if (mesh.axes.length < 2) return [];
            float[][] axes;
            axes.length = 2;
            axes[0] = mesh.axes[0].dup;
            axes[1] = mesh.axes[1].dup;
            return axes;
        } else if (auto deformable = cast(IncMeshEditorOneDeformable)impl) {
            return deriveAxes(deformable);
        }
        return [];
    }

    size_t vertexCount(IncMeshEditorOne impl) {
        if (auto drawable = cast(IncMeshEditorOneDrawable)impl) {
            return drawable.getMesh().vertices.length;
        } else if (auto deformable = cast(IncMeshEditorOneDeformable)impl) {
            return deformable.vertices.length;
        }
        return 0;
    }

    void applyAxes(IncMeshEditorOne impl, float[][] axes) {
        if (axes.length < 2) return;
        MeshData meshData;
        meshData.gridAxes = [axes[0].dup, axes[1].dup];
        meshData.regenerateGrid();

        if (auto drawable = cast(IncMeshEditorOneDrawable)impl) {
            auto mesh = drawable.getMesh();
            mesh.copyFromMeshData(meshData);
        } else if (auto deformable = cast(IncMeshEditorOneDeformable)impl) {
            auto newPositions = meshData.vertices;
            MeshVertex*[] newVerts;
            newVerts.length = newPositions.length;
            foreach (i, pos; newPositions) {
                MeshVertex* mv;
                if (i < deformable.vertices.length && deformable.vertices[i] !is null) {
                    mv = deformable.vertices[i];
                    mv.connections.length = 0;
                } else {
                    mv = new MeshVertex;
                }
                mv.position = pos;
                newVerts[i] = mv;
            }
            deformable.vertices = newVerts;
        }
        impl.vertexMapDirty = true;
    }

    bool isOnGridAxes(float[][] axes, int axis, vec2 mousePos, float threshold, out float value) {
        if (axes.length != 2 || axes[axis].length == 0 || axes[1-axis].length == 0) return false;
        float minBound = axes[1-axis][0] - threshold;
        float maxBound = axes[1-axis][$-1] + threshold;
        if (mousePos.vector[axis] < minBound || mousePos.vector[axis] > maxBound) return false;
        foreach (v; axes[axis]) {
            if (abs(mousePos.vector[1-axis] - v) < threshold) {
                value = v;
                return true;
            }
        }
        return false;
    }

    bool isOnEdgeAxes(float[][] axes, int axis, vec2 mousePos, float threshold, out float value) {
        if (axes.length != 2 || axes[axis].length == 0 || axes[1-axis].length == 0) return false;
        float minBound = axes[1-axis][0] - threshold;
        float maxBound = axes[1-axis][$-1] + threshold;
        if (mousePos.vector[axis] < minBound || mousePos.vector[axis] > maxBound) return false;
        float axisMin = axes[axis][0];
        float axisMax = axes[axis][$-1];
        if (abs(mousePos.vector[1-axis] - axisMin) < threshold) {
            value = axisMin;
            return true;
        } else if (abs(mousePos.vector[1-axis] - axisMax) < threshold) {
            value = axisMax;
            return true;
        }
        return false;
    }

    bool isOnGrid(IncMesh mesh, int axis, vec2 mousePos, float threshold, out float value) {
        return isOnGridAxes(mesh.axes, axis, mousePos, threshold, value);
    }
    bool isOnEdge(IncMesh mesh, int axis, vec2 mousePos, float threshold, out float value) {
        return isOnEdgeAxes(mesh.axes, axis, mousePos, threshold, value);
    }


    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        assert(!impl.deformOnly || toolMode != VertexToolMode.Grid);
        isDragging = false;
        impl.isSelecting = false;
        impl.deselectAll();
    }

    override bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
        if (auto implDrawable = cast(IncMeshEditorOneDrawable)(impl)) {
            return onDragStartDrawable(mousePos, impl, implDrawable);
        } else if (auto implDeformable = cast(IncMeshEditorOneDeformable)(impl)) {
            return onDragStartDeformable(mousePos, impl, implDeformable);
        }
        return false;
    }

    bool onDragStartDrawable(vec2 mousePos, IncMeshEditorOne impl, IncMeshEditorOneDrawable implDrawable) {
        auto mesh = implDrawable.getMesh();

        auto vtxAtMouse = impl.getVerticesByIndex([impl.vtxAtMouse])[0];
        if (vtxAtMouse) {
            if (mesh.axes.length != 2) {
                currentAction = GridActionID.End;
                return false;
            }

            currentAction = GridActionID.TranslateFree;
            dragOrigin = vtxAtMouse.position;

            float threshold = selectRadius/incViewportZoom;
            float xValue, yValue;
            bool foundY = isOnEdge(mesh, 0, dragOrigin, threshold, yValue);
            bool foundX = isOnEdge(mesh, 1, dragOrigin, threshold, xValue);

            if (foundY) {
                dragOrigin.y = yValue;
                if (!foundX) {
                    currentAction = GridActionID.TranslateX;
                } else {
                    dragTargetXIndex = cast(int)mesh.axes[1].countUntil(vtxAtMouse.position.x);
                    if (dragTargetXIndex < 0)
                        currentAction = GridActionID.End;
                }
            }

            if (foundX) {
                dragOrigin.x = xValue;
                if (!foundY) {
                    currentAction = GridActionID.TranslateY;
                } else {
                    dragTargetYIndex = cast(int)mesh.axes[0].countUntil(vtxAtMouse.position.y);
                    if (dragTargetYIndex < 0)
                        currentAction = GridActionID.End;
                }
            }
            if (!foundX) {
                dragTargetXIndex = cast(int)mesh.axes[1].countUntil(vtxAtMouse.position.x);
                if (dragTargetXIndex < 0)
                    currentAction = GridActionID.End;
            }
            if (!foundY) {
                dragTargetYIndex = cast(int)mesh.axes[0].countUntil(vtxAtMouse.position.y);
                if (dragTargetYIndex < 0)
                    currentAction = GridActionID.End;
            }

            return true;
        } else if (mesh.axes.length < 2 || mesh.vertices.length == 0) {
            currentAction = GridActionID.Create;
            dragOrigin = mousePos;
            return true;
        }
        return false;
    }

    bool onDragStartDeformable(vec2 mousePos, IncMeshEditorOne impl, IncMeshEditorOneDeformable implDeformable) {
        auto axes = currentAxes(impl);
        auto vtxAtMouse = impl.getVerticesByIndex([impl.vtxAtMouse])[0];
        if (vtxAtMouse && axes.length == 2) {
            currentAction = GridActionID.TranslateFree;
            dragOrigin = vtxAtMouse.position;

            float threshold = selectRadius/incViewportZoom;
            float xValue, yValue;
            bool foundY = isOnEdgeAxes(axes, 0, dragOrigin, threshold, yValue);
            bool foundX = isOnEdgeAxes(axes, 1, dragOrigin, threshold, xValue);

            if (foundY) {
                dragOrigin.y = yValue;
                if (!foundX) {
                    currentAction = GridActionID.TranslateX;
                } else {
                    dragTargetXIndex = cast(int)axes[1].countUntil(vtxAtMouse.position.x);
                    if (dragTargetXIndex < 0)
                        currentAction = GridActionID.End;
                }
            }

            if (foundX) {
                dragOrigin.x = xValue;
                if (!foundY) {
                    currentAction = GridActionID.TranslateY;
                } else {
                    dragTargetYIndex = cast(int)axes[0].countUntil(vtxAtMouse.position.y);
                    if (dragTargetYIndex < 0)
                        currentAction = GridActionID.End;
                }
            }
            if (!foundX) {
                dragTargetXIndex = cast(int)axes[1].countUntil(vtxAtMouse.position.x);
                if (dragTargetXIndex < 0)
                    currentAction = GridActionID.End;
            }
            if (!foundY) {
                dragTargetYIndex = cast(int)axes[0].countUntil(vtxAtMouse.position.y);
                if (dragTargetYIndex < 0)
                    currentAction = GridActionID.End;
            }

            return currentAction != GridActionID.End;
        } else if (axes.length < 2 || vertexCount(impl) == 0) {
            currentAction = GridActionID.Create;
            dragOrigin = mousePos;
            return true;
        }
        return false;
    }

    override bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl) {
        if (auto implDrawable = cast(IncMeshEditorOneDrawable)(impl)) {
            return onDragEndDrawable(mousePos, impl, implDrawable);
        } else if (auto implDeformable = cast(IncMeshEditorOneDeformable)(impl)) {
            return onDragEndDeformable(mousePos, impl, implDeformable);
        }
        return false;
    }

    bool onDragEndDrawable(vec2 mousePos, IncMeshEditorOne impl, IncMeshEditorOneDrawable implDrawable) {
        if (currentAction == GridActionID.TranslateX || currentAction == GridActionID.TranslateY || currentAction == GridActionID.TranslateFree) {
            currentAction = GridActionID.End;
            return true;
        } else if (currentAction == GridActionID.Create) {
            dragEnd = mousePos;
            vec4 bounds = vec4(min(dragOrigin.x, dragEnd.x), min(dragOrigin.y, dragEnd.y),
                               max(dragOrigin.x, dragEnd.x), max(dragOrigin.y, dragEnd.y));
            float width  = bounds.z - bounds.x;
            float height = bounds.w - bounds.y;

            auto mesh = implDrawable.getMesh();
            MeshData meshData;

            meshData.gridAxes = [[], []];
            for (int i = 0; i < numCut; i ++) {
                meshData.gridAxes[0] ~= bounds.y + height * i / (numCut - 1);
                meshData.gridAxes[1] ~= bounds.x + width  * i / (numCut - 1);
            }
            meshData.regenerateGrid();
            mesh.copyFromMeshData(meshData);
            impl.refreshMesh();
            currentAction = GridActionID.End;
            return true;
        }
        return false;
    }

    bool onDragEndDeformable(vec2 mousePos, IncMeshEditorOne impl, IncMeshEditorOneDeformable implDeformable) {
        if (currentAction == GridActionID.TranslateX || currentAction == GridActionID.TranslateY || currentAction == GridActionID.TranslateFree) {
            currentAction = GridActionID.End;
            return true;
        } else if (currentAction == GridActionID.Create) {
            dragEnd = mousePos;
            vec4 bounds = vec4(min(dragOrigin.x, dragEnd.x), min(dragOrigin.y, dragEnd.y),
                               max(dragOrigin.x, dragEnd.x), max(dragOrigin.y, dragEnd.y));
            float width  = bounds.z - bounds.x;
            float height = bounds.w - bounds.y;

            float[][] axes;
            axes.length = 2;
            axes[0] = [];
            axes[1] = [];
            for (int i = 0; i < numCut; i ++) {
                axes[0] ~= bounds.y + height * i / (numCut - 1);
                axes[1] ~= bounds.x + width  * i / (numCut - 1);
            }
            applyAxes(impl, axes);
            impl.refreshMesh();
            currentAction = GridActionID.End;
            return true;
        }
        return false;
    }

    override bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl) {
        dragEnd = impl.mousePos;
        if (auto implDrawable = cast(IncMeshEditorOneDrawable)(impl)) {
            return onDragUpdateDrawable(mousePos, impl, implDrawable);
        } else if (auto implDeformable = cast(IncMeshEditorOneDeformable)(impl)) {
            return onDragUpdateDeformable(mousePos, impl, implDeformable);
        }
        return false;
    }

    bool onDragUpdateDrawable(vec2 mousePos, IncMeshEditorOne impl, IncMeshEditorOneDrawable implDrawable) {
        auto mesh = implDrawable.getMesh();

        if (currentAction == GridActionID.TranslateX) {
            mesh.axes[1][dragTargetXIndex] = mousePos.x;
            mesh.axes[1].sort();
            dragTargetXIndex = cast(int)mesh.axes[1].countUntil(mousePos.x);
            MeshData meshData;
            meshData.gridAxes = mesh.axes[];
            meshData.regenerateGrid();
            mesh.copyFromMeshData(meshData);
            impl.refreshMesh();
            return true;
        } else if (currentAction == GridActionID.TranslateY) {
            mesh.axes[0][dragTargetYIndex] = mousePos.y;
            mesh.axes[0].sort();
            dragTargetYIndex = cast(int)mesh.axes[0].countUntil(mousePos.y);
            MeshData meshData;
            meshData.gridAxes = mesh.axes[];
            meshData.regenerateGrid();
            mesh.copyFromMeshData(meshData);
            impl.refreshMesh();
            return true;
        } else if (currentAction == GridActionID.TranslateFree) {
            mesh.axes[0][dragTargetYIndex] = mousePos.y;
            mesh.axes[0].sort();
            dragTargetYIndex = cast(int)mesh.axes[0].countUntil(mousePos.y);

            mesh.axes[1][dragTargetXIndex] = mousePos.x;
            mesh.axes[1].sort();
            dragTargetXIndex = cast(int)mesh.axes[1].countUntil(mousePos.x);
            MeshData meshData;
            meshData.gridAxes = mesh.axes[];
            meshData.regenerateGrid();
            mesh.copyFromMeshData(meshData);
            impl.refreshMesh();
            return true;
        } else if (currentAction == GridActionID.Create) {
            return true;
        }

        return false;
    }

    bool onDragUpdateDeformable(vec2 mousePos, IncMeshEditorOne impl, IncMeshEditorOneDeformable implDeformable) {
        auto axes = currentAxes(impl);
        if (axes.length != 2) return false;

        if (currentAction == GridActionID.TranslateX) {
            axes[1][dragTargetXIndex] = mousePos.x;
            axes[1].sort();
            dragTargetXIndex = cast(int)axes[1].countUntil(mousePos.x);
            applyAxes(impl, axes);
            impl.refreshMesh();
            return true;
        } else if (currentAction == GridActionID.TranslateY) {
            axes[0][dragTargetYIndex] = mousePos.y;
            axes[0].sort();
            dragTargetYIndex = cast(int)axes[0].countUntil(mousePos.y);
            applyAxes(impl, axes);
            impl.refreshMesh();
            return true;
        } else if (currentAction == GridActionID.TranslateFree) {
            axes[0][dragTargetYIndex] = mousePos.y;
            axes[0].sort();
            dragTargetYIndex = cast(int)axes[0].countUntil(mousePos.y);

            axes[1][dragTargetXIndex] = mousePos.x;
            axes[1].sort();
            dragTargetXIndex = cast(int)axes[1].countUntil(mousePos.x);
            applyAxes(impl, axes);
            impl.refreshMesh();
            return true;
        } else if (currentAction == GridActionID.Create) {
            return true;
        }
        return false;
    }

    bool updateMeshEdit(ImGuiIO* io, IncMeshEditorOne impl, out bool changed) {
        if (auto implDrawable = cast(IncMeshEditorOneDrawable)(impl)) {
            return updateMeshEditDrawable(io, impl, implDrawable, changed);
        } else if (auto implDeformable = cast(IncMeshEditorOneDeformable)(impl)) {
            return updateMeshEditDeformable(io, impl, implDeformable, changed);
        }
        changed = false;
        return false;
    }

    bool updateMeshEditDrawable(ImGuiIO* io, IncMeshEditorOne impl, IncMeshEditorOneDrawable implDrawable, out bool changed) {
        auto mesh = implDrawable.getMesh();

        if (isDragging && incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
            isDragging = false;
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);

        incStatusTooltip(_("Drag to define 2x2 mesh"), _("Left Mouse"));
        incStatusTooltip(_("Add/remove key points to axes"), _("Left Mouse"));
        incStatusTooltip(_("Change key point position in the axis"), _("Left Mouse"));

        if (!isDragging && incInputIsMouseReleased(ImGuiMouseButton.Left) && impl.maybeSelectOne != ulong(-1)) {
            impl.selectOne(impl.maybeSelectOne);
        }

        // Left double click action
        if (igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
            auto vtxAtMouse = impl.getVerticesByIndex([impl.vtxAtMouse])[0];
            if (vtxAtMouse !is null) {
                // Remove axis point from gridAxes
                float x = vtxAtMouse.position.x;
                float y = vtxAtMouse.position.y;
                if (mesh.axes.length == 2) {
                    auto ycount = mesh.axes[0].countUntil(y);
                    auto xcount = mesh.axes[1].countUntil(x);
                    if ((xcount == 0 || xcount == mesh.axes[1].length - 1) &&
                        (ycount == 0 || ycount == mesh.axes[0].length - 1)) {
                    } else if (xcount == 0 || xcount == mesh.axes[1].length - 1) {
                        // Removes only y axis
                        mesh.axes[0] = mesh.axes[0].remove(ycount);
                    } else if (ycount == 0 || ycount == mesh.axes[0].length - 1) {
                        // Removes only x axis
                        mesh.axes[1] = mesh.axes[1].remove(xcount);
                    } else {
                        mesh.axes[0] = mesh.axes[0].remove(ycount);
                        mesh.axes[1] = mesh.axes[1].remove(xcount);
                    }
                    MeshData meshData;
                    meshData.gridAxes = mesh.axes[];
                    meshData.regenerateGrid();
                    mesh.copyFromMeshData(meshData);
                    impl.refreshMesh();
                    impl.vtxAtMouse = ulong(-1);
                }

            } else {
                // Add axis point to grid Axes
                if (mesh.axes.length == 2) {
                    float x, y;
                    float threshold = selectRadius/incViewportZoom;
                    auto mousePos = impl.mousePos;
                    float yValue, xValue;
                    bool foundY = isOnGrid(mesh, 0, mousePos, threshold, yValue);
                    bool foundX = isOnGrid(mesh, 1, mousePos, threshold, xValue);

                    if (!foundY) {
                        y = mousePos.y;
                        for (int i = 0; i < mesh.axes[0].length; i ++)
                            if (y < mesh.axes[0][i]) {
                                mesh.axes[0].insertInPlace(i, y);
                                break;
                            }
                    }
                    if (!foundX) {
                        x = mousePos.x;
                        for (int i = 0; i < mesh.axes[1].length; i ++)
                            if (x < mesh.axes[1][i]) {
                                mesh.axes[1].insertInPlace(i, x);
                                break;
                            }
                    }
                    MeshData meshData;
                    meshData.gridAxes = mesh.axes[];
                    meshData.regenerateGrid();
                    mesh.copyFromMeshData(meshData);
                    impl.refreshMesh();
                }
            }

        }

        // Dragging
        if (!isDragging && incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            onDragStart(impl.mousePos, impl);
            isDragging = true;
        }
        if (isDragging)
            onDragUpdate(impl.mousePos, impl);

        changed = false;
        return true;
    }

    bool updateMeshEditDeformable(ImGuiIO* io, IncMeshEditorOne impl, IncMeshEditorOneDeformable implDeformable, out bool changed) {
        auto axes = currentAxes(impl);

        if (isDragging && incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
            isDragging = false;
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);

        incStatusTooltip(_("Drag to define 2x2 mesh"), _("Left Mouse"));
        incStatusTooltip(_("Add/remove key points to axes"), _("Left Mouse"));
        incStatusTooltip(_("Change key point position in the axis"), _("Left Mouse"));

        if (!isDragging && incInputIsMouseReleased(ImGuiMouseButton.Left) && impl.maybeSelectOne != ulong(-1)) {
            impl.selectOne(impl.maybeSelectOne);
        }

        // Left double click action
        if (igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
            auto vtxAtMouse = impl.getVerticesByIndex([impl.vtxAtMouse])[0];
            if (vtxAtMouse !is null && axes.length == 2) {
                float x = vtxAtMouse.position.x;
                float y = vtxAtMouse.position.y;
                auto ycount = axes[0].countUntil(y);
                auto xcount = axes[1].countUntil(x);
                if ((xcount == 0 || xcount == axes[1].length - 1) &&
                    (ycount == 0 || ycount == axes[0].length - 1)) {
                } else if (xcount == 0 || xcount == axes[1].length - 1) {
                    axes[0] = axes[0].remove(ycount);
                } else if (ycount == 0 || ycount == axes[0].length - 1) {
                    axes[1] = axes[1].remove(xcount);
                } else {
                    axes[0] = axes[0].remove(ycount);
                    axes[1] = axes[1].remove(xcount);
                }
                applyAxes(impl, axes);
                impl.refreshMesh();
                impl.vtxAtMouse = ulong(-1);
                axes = currentAxes(impl);
            } else if (axes.length == 2) {
                // Add axis point to grid Axes
                float threshold = selectRadius/incViewportZoom;
                auto mousePos = impl.mousePos;
                float yValue, xValue;
                bool foundY = isOnGridAxes(axes, 0, mousePos, threshold, yValue);
                bool foundX = isOnGridAxes(axes, 1, mousePos, threshold, xValue);

                if (!foundY) {
                    float y = mousePos.y;
                    bool inserted = false;
                    for (int i = 0; i < axes[0].length; i ++) {
                        if (y < axes[0][i]) {
                            axes[0].insertInPlace(i, y);
                            inserted = true;
                            break;
                        }
                    }
                    if (!inserted) axes[0] ~= y;
                }
                if (!foundX) {
                    float x = mousePos.x;
                    bool inserted = false;
                    for (int i = 0; i < axes[1].length; i ++) {
                        if (x < axes[1][i]) {
                            axes[1].insertInPlace(i, x);
                            inserted = true;
                            break;
                        }
                    }
                    if (!inserted) axes[1] ~= x;
                }
                applyAxes(impl, axes);
                impl.refreshMesh();
                axes = currentAxes(impl);
            }
        }

        // Dragging
        if (!isDragging && incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            onDragStart(impl.mousePos, impl);
            isDragging = true;
        }
        if (isDragging)
            onDragUpdate(impl.mousePos, impl);

        changed = false;
        return true;
    }

    override bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        super.update(io, impl, action, changed);

        if (!impl.deformOnly)
            updateMeshEdit(io, impl, changed);
        return changed;
    }

    override void draw (Camera camera, IncMeshEditorOne impl) {
        if (currentAction == GridActionID.Create) {
            vec3[] lines;
            vec4 color = vec4(0.2, 0.9, 0.9, 1);

            vec4 bounds = vec4(min(dragOrigin.x, dragEnd.x), min(dragOrigin.y, dragEnd.y),
                               max(dragOrigin.x, dragEnd.x), max(dragOrigin.y, dragEnd.y));
            float width  = bounds.z - bounds.x;
            float height = bounds.w - bounds.y;
            
            for (int i;  i < numCut; i ++) {
                float offy = bounds.y + height * i / (numCut - 1);
                float offx = bounds.x + width  * i / (numCut - 1);
                lines ~= [vec3(bounds.x, offy, 0), vec3(bounds.z, offy, 0)];
                lines ~= [vec3(offx, bounds.y, 0), vec3(offx, bounds.w, 0)];
            }
            inDbgSetBuffer(lines);
            inDbgDrawLines(color, mat4.identity());

        } else if (currentAction == GridActionID.TranslateX || currentAction == GridActionID.TranslateY || currentAction == GridActionID.TranslateFree) {

        } else {

        }
    }
}

class ToolInfoImpl(T: GridTool) : ToolInfoBase!(T) {
    override
    void setupToolMode(IncMeshEditorOne e, VertexToolMode mode) {
        e.setToolMode(mode);
        e.setPath(null);
        e.deforming = false;
        e.refreshMesh();
    }

    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (deformOnly)
            return false;

        auto targets = editors.keys();
        bool supported = targets.length == 0 || targets.all!(k => cast(Drawable)k !is null || cast(GridDeformer)k !is null);
        if (!supported) return false;
        return super.viewportTools(deformOnly, toolMode, editors);
    }
    override bool canUse(bool deformOnly, Node[] targets) {
        if (!super.canUse(deformOnly, targets)) return false;
        if (deformOnly) return false;
        return targets.all!(k => cast(Drawable)k !is null || cast(GridDeformer)k !is null);
    }
    override VertexToolMode mode() { return VertexToolMode.Grid; };
    override string icon() { return "";}
    override string description() { return _("Grid Vertex Tool");}
}
