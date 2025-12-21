/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport.model;
import nijigenerate.viewport.model.deform;
import nijigenerate.widgets.tooltip;
import nijigenerate.widgets.label;
import nijigenerate.widgets.texture;
import nijigenerate.widgets.dummy;
import nijigenerate.widgets.dragdrop;
import nijigenerate.widgets.button;
import nijigenerate.core.input;
import nijigenerate.core;
import nijigenerate.viewport.base;
import nijigenerate.viewport.vertex;
import nijigenerate.viewport.model.onionslice;
import nijigenerate;
import nijilive;
import nijilive.core.nodes.drivers; // Driver
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijigenerate.core.dbg;
import bindbc.imgui;
import i18n;
//import std.stdio;
import std.algorithm;
import std.algorithm.iteration : uniq;
import std.array;

private {
    Part[] foundParts;

    enum ENTRY_SIZE = 48;
}

/** 
    For detailed puppets or small components, we assume the user will directly point to the one they want. 
    ZSort is slightly less effective than SizeSort on layers with more alpha pixels (e.g., hair overlapping eyes). 
    SizeSort prioritizes smaller objects, making them easier to select.
*/
enum ViewporMenuSortMode {
    ZSort,
    SizeSort,
}

ViewporMenuSortMode incViewportModelMenuSortMode = ViewporMenuSortMode.ZSort;

class ModelViewport : DelegationViewport {
public:

    override
    void draw(Camera camera) { 
        Parameter param = incArmedParameter();
        incActivePuppet.update();
        incActivePuppet.draw();
        auto onion = OnionSlice.singleton();
        onion.draw();

        if (subView) {
            subView.draw(camera);
        } else {
            if (incSelectedNodes.length > 0) {
                foreach(selectedNode; incSelectedNodes) {
                    if (selectedNode is null) continue; 
                    if (incShowOrientation) selectedNode.drawOrientation();
                    if (incShowBounds) selectedNode.drawBounds();

                    if (Drawable selectedDraw = cast(Drawable)selectedNode) {

                        if (incShowVertices || incEditMode != EditMode.ModelEdit) {
                            selectedDraw.drawMeshLines();
                        }
                    } else if (auto deformable = cast(PathDeformer)selectedNode) {

                        /**
                            Draws the mesh
                        */
                        void drawLines(Curve curve, mat4 trans = mat4.identity, vec4 color = vec4(0.5, 1, 0.5, 1)) {
                            if (curve is null || curve.controlPoints.length == 0)
                                return;
                            Vec3Array lines;
                            foreach (i; 1..100) {
                                lines ~= vec3(curve.point((i - 1) / 100.0), 0);
                                lines ~= vec3(curve.point(i / 100.0), 0);
                            }
                            if (lines.length > 0) {
                                inDbgSetBuffer(lines);
                                inDbgDrawLines(color, trans);
                            }
                        }
                        drawLines(deformable.prevCurve, deformable.transform.matrix, vec4(0.5, 0.5, 0.5, 1));
                        drawLines(deformable.deformedCurve, deformable.transform.matrix, vec4(0.5, 1, 0.5, 1));
                        debug(path_deform) {
                            void drawLines2(Vec2Array[Node] closestPoints, mat4 trans, vec4 color) {
                                if (closestPoints.length == 0)
                                    return;
                                Vec3Array lines;
                                foreach (t2, deformed; closestPoints) {
                                    if (auto deformable2 = cast(Deformable)t2) {
                                        mat4 conv = trans.inverse * deformable2.transform.matrix;
                                        foreach (i, v; deformable2.vertices) {
                                            lines ~= vec3((conv * vec4(v, 0, 1)).xy, 0);
                                            lines ~= vec3(deformed[i], 0);
                                        }
                                    }
                                }
                                if (lines.length > 0) {
                                    inDbgSetBuffer(lines);
                                    inDbgDrawLines(color, trans);
                                }
                            }
                            drawLines2(deformable.closestPointsOriginal, deformable.transform.matrix, vec4(0.5, 1, 1, 1));
                            void drawLines3(Vec2Array[Node] closestPoints, mat4 trans, vec4 color) {
                                if (closestPoints.length == 0)
                                    return;
                                Vec3Array lines;
                                foreach (t, deformed; closestPoints) {
                                    if (auto deformable2 = cast(Deformable)t) {
                                        mat4 conv = trans.inverse * deformable2.transform.matrix;
                                        auto vertCount = deformable2.vertices.length;
                                        auto deformCount = deformable2.deformation.length;
                                        foreach (i; 0 .. vertCount) {
                                            vec2 v = deformable2.vertices[i].toVector();
                                            if (i < deformCount) v += deformable2.deformation[i].toVector();
                                            lines ~= vec3((conv * vec4(v, 0, 1)).xy, 0);
                                            lines ~= vec3(deformed[i], 0);
                                        }
                                    }
                                }
                                if (lines.length > 0) {
                                    inDbgSetBuffer(lines);
                                    inDbgDrawLines(color, trans);
                                }
                            }
                            drawLines3(deformable.closestPointsDeformed, deformable.transform.matrix, vec4(0.5, 1, 0.5, 1));
                        }
                    } else if (auto grid = cast(GridDeformer)selectedNode) {
                        auto baseVerts = grid.vertices;
                        if (baseVerts.length >= 4) {
                            auto baseVertsAoS = baseVerts.toArray();
                            auto xs = baseVertsAoS.map!(v => v.x).array;
                            auto ys = baseVertsAoS.map!(v => v.y).array;
                            xs.sort();
                            ys.sort();
                            xs = xs.uniq.array;
                            ys = ys.uniq.array;
                            size_t cols = xs.length;
                            size_t rows = ys.length;
                            bool haveDeform = grid.deformation.length == baseVerts.length;
                            if (cols >= 2 && rows >= 2 && cols * rows == baseVerts.length) {
                                Vec3Array lines;
                                foreach (y; 0 .. rows) {
                                    foreach (x; 0 .. cols) {
                                        size_t idx = y * cols + x;
                                        vec2 startPos = baseVertsAoS[idx];
                                        if (haveDeform) startPos += grid.deformation[idx];
                                        auto start = vec3(startPos, 0);
                                        if (x + 1 < cols) {
                                            size_t nextIdx = idx + 1;
                                            vec2 rightPos = baseVertsAoS[nextIdx];
                                            if (haveDeform) rightPos += grid.deformation[nextIdx];
                                            lines ~= start;
                                            lines ~= vec3(rightPos, 0);
                                        }
                                        if (y + 1 < rows) {
                                            size_t nextIdx = idx + cols;
                                            vec2 downPos = baseVertsAoS[nextIdx];
                                            if (haveDeform) downPos += grid.deformation[nextIdx];
                                            lines ~= start;
                                            lines ~= vec3(downPos, 0);
                                        }
                                    }
                                }
                                if (lines.length > 0) {
                                    inDbgSetBuffer(lines);
                                    inDbgDrawLines(vec4(0.5, 0.5, 0.5, 1), grid.transform.matrix);
                                }
                            }
                        }
                    }
                    if (Driver selectedDriver = cast(Driver)selectedNode) {
                        selectedDriver.drawDebug();
                    }
                }
            }
        }
    };
    
    override
    void drawOptions() {
        if (!incArmedParameter()) {
            if(incBeginDropdownMenu("GIZMOS", "")) {

                if (incButtonColored("", ImVec2(0, 0), incShowVertices ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                    incShowVertices = !incShowVertices;
                }
                incTooltip(incShowVertices ? _("Hide Vertices") : _("Show Vertices"));
                    
                igSameLine(0, 4);
                if (incButtonColored("", ImVec2(0, 0), incShowBounds ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                    incShowBounds = !incShowBounds;
                }
                incTooltip(incShowBounds ? _("Hide Bounds") : _("Show Bounds"));

                igSameLine(0, 4);
                if (incButtonColored("", ImVec2(0, 0), incShowOrientation ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                    incShowOrientation = !incShowOrientation;
                }
                incTooltip(incShowOrientation ? _("Hide Orientation Gizmo") : _("Show Orientation Gizmo"));

                // DropdownMenu is silly, so
                igSameLine(0, 0);
                incDummy(ImVec2(4, 0));
                igSameLine(0, 0);
                if(incBeginDropdownMenu("COLOR", "", ImVec2(128, 0), ImVec2(float.max, float.max))) {
                    import nijilive : inSetClearColor, inGetClearColor;

                    // Get clear color
                    vec3 clearColor;
                    float a = 1;
                    inGetClearColor(clearColor.r, clearColor.g, clearColor.b, a);

                    // Set clear color
                    igColorPicker3(__("COLOR"), &clearColor.vector, 
                        ImGuiColorEditFlags.NoSidePreview | 
                        ImGuiColorEditFlags.NoLabel |
                        ImGuiColorEditFlags.NoSmallPreview |
                        ImGuiColorEditFlags.NoBorder
                    );
                    ImVec2 space = incAvailableSpace();

                    inSetClearColor(clearColor.r, clearColor.g, clearColor.b, a);
                    incDummy(ImVec2(0, 4));

                    if (incButtonColored(__("Reset"), ImVec2(space.x, 0))) incResetClearColor();
                    
                    incEndDropdownMenu();
                }
                incTooltip(_("Background Color"));

                incEndDropdownMenu();
            }
            incTooltip(_("Gizmos"));
        } else {
            super.drawOptions();
        }
    };
 
    override
    void drawConfirmBar() {

        // If parameter is armed we should *not* show the edit mesh button
        if (subView) return;

        igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(16, 4));
            if (Deformable node = cast(Deformable)incSelectedNode()) {
                auto io = igGetIO();
                const(char)* text = incHasDragDrop("_PUPPETNTREE") ? (io.KeyCtrl ? __(" Merge Mesh"): __(" Copy Mesh")) : __(" Edit Mesh");
                
                if (incButtonColored(text, ImVec2(0, 26))) {
                    incVertexEditStartEditing(node);
                }

                // Allow copying mesh data via drag n drop for now
                if(igBeginDragDropTarget()) {
                    if (io.KeyCtrl)
                        incTooltip(_("Merge Mesh Data"));
                    else
                        incTooltip(_("Copy Mesh Data"));
                    
                    const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
                    if (payload !is null) {
                        if (auto payloadDeformable = cast(Deformable)*cast(Node*)payload.Data) {
                            incSetEditMode(EditMode.VertexEdit);
                            incSelectNode(node);
                            incVertexEditSetTarget(node);
                            incFocusCamera(node, vec2(0, 0));
                            if (io.KeyCtrl) {
                                incVertexEditMergeMeshDataToTarget(node, payloadDeformable);

                            } else {
                                incVertexEditCopyMeshDataToTarget(node, payloadDeformable);
                            }
                        }
                    }
                    igEndDragDropTarget();
                } else {
                    // Switches nijigenerate over to Mesh Edit mode
                    // and selects the mesh that you had selected previously
                    // in Model Edit mode.
                    incTooltip(_("Edit Mesh"));
                }
            }
        igPopStyleVar();        
    };

    override
    void menu() {
        if (subView) subView.menu();

        if (igMenuItem(incViewportModelMenuSortMode == ViewporMenuSortMode.ZSort ? __("Z-Sort") : __("Size-Sort"))) {
            if (incViewportModelMenuSortMode == ViewporMenuSortMode.ZSort)
                incViewportModelMenuSortMode = ViewporMenuSortMode.SizeSort;
            else
                incViewportModelMenuSortMode = ViewporMenuSortMode.ZSort;
        }

        igSeparator();
        
        if (incSelectedNode() != incActivePuppet().root) {
            if (igMenuItem(__("Focus Selected"))) {
                incFocusCamera(incSelectedNode());
            }
        }

        if (igBeginChild("FOUND_PARTS", ImVec2(256, 256), false)) {
            if (foundParts.length > 0) {
                ImVec2 avail = incAvailableSpace();
                ImVec2 cursorPos;
                foreach(Part part; foundParts) {
                    igPushID(part.uuid);
                        ImVec2 nameSize = incMeasureString(part.name);

                        // Selectable
                        igGetCursorPos(&cursorPos);
                        if (igSelectable("###PartSelectable", false, ImGuiSelectableFlags.None, ImVec2(avail.x, ENTRY_SIZE))) {
                            
                            // Add selection if ctrl is down, otherwise set selection
                            if (igIsKeyDown(ImGuiKey.LeftCtrl) || igIsKeyDown(ImGuiKey.RightCtrl)) incAddSelectNode(part);
                            else incSelectNode(part);

                            // Escape early, we're already done.
                            igPopID();
                            igEndChild();
                            igCloseCurrentPopup();
                            return;
                        }
                        igSetItemAllowOverlap();

                        if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                            igSetDragDropPayload("_PUPPETNTREE", cast(void*)&part, (&part).sizeof, ImGuiCond.Always);
                            incDragdropNodeList(part);
                            igEndDragDropSource();
                        }

                        // ICON
                        igSetCursorPos(ImVec2(cursorPos.x+2, cursorPos.y+2));
                        incTextureSlotUntitled("ICON", part.textures[0], ImVec2(ENTRY_SIZE-4, ENTRY_SIZE-4), 24, ImGuiWindowFlags.NoInputs);
                        
                        // Name
                        igSetCursorPos(ImVec2(cursorPos.x + ENTRY_SIZE + 4, cursorPos.y + (ENTRY_SIZE/2) - (nameSize.y/2)));
                        incText(part.name);

                        // Move to next line
                        igSetCursorPos(ImVec2(cursorPos.x, cursorPos.y + ENTRY_SIZE + 3));
                    igPopID();
                }
            } else {
                incText(_("No parts found"));
            }
        }
        igEndChild();        
    };

    override
    void menuOpening() { 
        foundParts.length = 0;

        vec2 mpos = incInputGetMousePosition()*-1;
        mloop: foreach(ref Part part; incActivePuppet.getAllParts()) {
            rect b = rect(part.bounds.x, part.bounds.y, part.bounds.z-part.bounds.x, part.bounds.w-part.bounds.y);
            if (b.intersects(mpos)) {

                // Skip already selected parts
                foreach(pn; incSelectedNodes()) {
                    if (pn.uuid == part.uuid) continue mloop;
                }
                foundParts ~= part;
            }
        }

        import std.algorithm.sorting : sort;
        import std.algorithm.mutation : SwapStrategy;
        import std.math : cmp;

        if (incViewportModelMenuSortMode == ViewporMenuSortMode.ZSort) {
            sort!((a, b) => cmp(
                a.zSortNoOffset, 
                b.zSortNoOffset) < 0, SwapStrategy.stable)(foundParts);

        } else if (incViewportModelMenuSortMode == ViewporMenuSortMode.SizeSort) {
            sort!((a, b) => cmp(
                (a.bounds.z - a.bounds.x) * (a.bounds.w - a.bounds.y), 
                (b.bounds.z - b.bounds.x) * (b.bounds.w - b.bounds.y)) < 0, SwapStrategy.stable)(foundParts);

        } else {
            throw new Exception("Unknown sort mode");
        }
    };

    override
    bool hasMenu() { return true; }

    override
    void armedParameterChanged(Parameter parameter) {
        if (parameter && subView is null) {
            subView = new DeformationViewport;
            subView.selectionChanged(incSelectedNodes);
        } else if (parameter is null && subView) {
            subView = null;
        }
        if (subView)
            subView.armedParameterChanged(parameter);
    }
}
