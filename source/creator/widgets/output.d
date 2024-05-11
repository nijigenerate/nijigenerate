module creator.widgets.output;

import std.stdio;
import std.array;
import std.string;
import std.math;
import std.algorithm;
import i18n;
import inochi2d;
import creator;
import creator.core;
import creator.core.selector;
import creator.panels;
import creator.utils;
import creator.widgets;
import creator.panels.shell;
import creator.panels.inspector;
import creator.panels.parameters;
import creator.panels.nodes;
import creator.ext;

private {
    string parameterGrabStr;
    enum NodeViewWidth = 250;

    void onNodeView(Node node) {
        if (node !is null && node != incActivePuppet().root) {
            // Per-edit mode inspector drawers
            switch(incEditMode()) {
                case EditMode.ModelEdit:
                    if (incArmedParameter()) {
                        Parameter param = incArmedParameter();
                        vec2u cursor = param.findClosestKeypoint();
                        incCommonNonEditHeader(node);
                        incInspectorDeformTRS(node, param, cursor);

                        // Node Part Section
                        if (Part part = cast(Part)node) {
                            incInspectorDeformPart(part, param, cursor);
                        }

                        if (Composite composite = cast(Composite)node) {
                            incInspectorDeformComposite(composite, param, cursor);
                        }

                        if (SimplePhysics phys = cast(SimplePhysics)node) {
                            incInspectorDeformSimplePhysics(phys, param, cursor);
                        }

                    } else {
                        incModelModeHeader(node);
                        incInspectorModelTRS(node);

                        // Node Camera Section
                        if (ExCamera camera = cast(ExCamera)node) {
                            incInspectorModelCamera(camera);
                        }

                        // Node Drawable Section
                        if (Composite composite = cast(Composite)node) {
                            incInspectorModelComposite(composite);
                        }


                        // Node Drawable Section
                        if (Drawable drawable = cast(Drawable)node) {
                            incInspectorModelDrawable(drawable);
                        }

                        // Node Part Section
                        if (Part part = cast(Part)node) {
                            incInspectorModelPart(part);
                        }

                        // Node SimplePhysics Section
                        if (SimplePhysics part = cast(SimplePhysics)node) {
                            incInspectorModelSimplePhysics(part);
                        }

                        // Node MeshGroup Section
                        if (MeshGroup group = cast(MeshGroup)node) {
                            incInspectorModelMeshGroup(group);
                        }
                    }
                
                break;
                default:
                    incCommonNonEditHeader(node);
                    break;
            }
        } else incInspectorModelInfo();        
    }

    void onParameterView(ulong index, Parameter param) {
        incParameterView!(false, false, true)(cast(int)index, param, &parameterGrabStr, false, incActivePuppet.parameters);
    }

    void setTransparency(float alpha, float text) {
        ImGuiCol[] colIDs = [ImGuiCol.WindowBg, ImGuiCol.Text, ImGuiCol.FrameBg, ImGuiCol.Button, ImGuiCol.Border, ImGuiCol.PopupBg];
        foreach (id; colIDs) {
            ImVec4 style;
            style = *igGetStyleColorVec4(id);
            style.w = id == ImGuiCol.Text? text: alpha;
            igPushStyleColor(id, style);
        }
    }

    bool isWindowHovered() {
        if (igIsWindowHovered(ImGuiHoveredFlags.ChildWindows)) return true;
        ImVec2 pos, size;
        ImVec2 curPos;
        igGetMousePos(&curPos);
        igGetWindowPos(&pos);
        igGetWindowSize(&size);
        return pos.x <= curPos.x && curPos.x < pos.x + size.x &&
               pos.y <= curPos.y && curPos.y < pos.y + size.y;
    }

    ImRect getOuterRect(ImGuiWindow* window) {
        ImRect r_outer;
        igGetPopupAllowedExtentRect(&r_outer, window);
        return r_outer;
    }

    void adjustWindowPos(bool largePart)(ImVec2 minPos, ImVec2 maxPos, ImGuiWindow* window) {
        if (window is null || window.RootWindow is null) return;
        ImRect r_outer = getOuterRect(window);
        float spaceLeft = minPos.x - r_outer.Min.x;
        float spaceRight = r_outer.Max.x - r_outer.Min.x - maxPos.x;
        float spaceUp = minPos.y - r_outer.Min.y;
        float spaceDown = r_outer.Max.y - r_outer.Min.y - minPos.y;

//        writefln("adjust: window: %.2f, %.2f, %.2f, %.2f", window.Pos.x, window.Pos.y, window.Size.x, window.Size.y);
//        writefln("adjust: root: %.2f, %.2f, %.2f, %.2f", r_outer.Min.x, r_outer.Min.y, r_outer.Max.x, r_outer.Max.y);

        if (largePart) {
            if (spaceLeft > spaceRight) {
                igSetWindowPos(window, ImVec2(minPos.x - window.Size.x, minPos.y));
            } else {
                igSetWindowPos(window, ImVec2(maxPos.x, minPos.y));
            }
        } else {
            if (spaceLeft > spaceRight) {
                igSetWindowPos(window, ImVec2(maxPos.x, minPos.y));
            } else {
                igSetWindowPos(window, ImVec2(minPos.x - window.Size.x, minPos.y));
            }
        }
        if (window.Size.y >= spaceDown) {
            igSetWindowPos(window, ImVec2(window.Pos.x, window.Pos.y - (window.Size.y - spaceDown)));
        }
    }
}

interface CommandIssuer {
    void addCommand(string);
    void setCommand(string);
    void addPopup(Resource);
}

interface Output {
    void onUpdate();
}


class TreeOutput : Output {
protected:
    int IconSize = 20;
    CommandIssuer panel;
    Resource popupOpened = null;

    Resource[] nodes;
    Resource[][Resource] children;
    Resource[] roots;
    bool[Resource] nodeIncluded;
    Resource focused = null;
    bool[uint] contentsDrawn;

    void showContents(Resource res) {
        ImVec2 size = ImVec2(20, 20);
        if (igButton("\ue763", size)) {
            if (panel) {
                panel.addPopup(res);
                popupOpened = null;
            }
        }

        if (res.type == ResourceType.Node) {
            Node node = to!Node(res);
            igSameLine();
            if (igButton("\ue8b8", size)) {
                igOpenPopup("NodeActionsPopup2");
            }
            bool isRoot = node.parent is null;
            if (isRoot)
                incNodeActionsPopup!("NodeActionsPopup2", true, true)(node);
            else
                incNodeActionsPopup!("NodeActionsPopup2", false, true)(node);
            igSameLine();
            igText(res.name.toStringz);
            onNodeView(node);
        } else if (res.type == ResourceType.Parameter) {
            Parameter param = to!Parameter(res);
            igSameLine();
            incParameterViewEditButtons!(false, true)(res.index, param, incActivePuppet.parameters, true);
            igSameLine();
            igText(res.name.toStringz);
            onParameterView(res.index, param);
        }
    };

    ImGuiTreeNodeFlags setFlag(Resource res) {
        ImGuiTreeNodeFlags flags;
        if (res !in children && res.type != ResourceType.Parameter) flags |= ImGuiTreeNodeFlags.Leaf;
        flags |= ImGuiTreeNodeFlags.DefaultOpen;
        flags |= ImGuiTreeNodeFlags.OpenOnArrow;
        return flags;
    }

    void drawTreeItem(Resource res) {
        bool isNode = res.type == ResourceType.Node;
        bool selected = false;
        bool noIcon = false;
        Node node;
        igSameLine();
        if (isNode) {
            node = to!Node(res);
            selected = incNodeInSelection(node);
            if (auto part = cast(Part)node) {
                if (node.typeId == "Part") {
                    noIcon = true;
                    incTextureSlotUntitled("ICON", part.textures[0], ImVec2(IconSize, IconSize), 1, ImGuiWindowFlags.NoInputs);
                }
                igSameLine();
            }
        }
        if (res !in nodeIncluded) {
            igPushStyleColor(ImGuiCol.Text, igGetStyle().Colors[ImGuiCol.TextDisabled]);
        }

        bool isActive = false;
        switch (res.type) {
        case ResourceType.Parameter:
            auto param = to!Parameter(res);
            isActive = incArmedParameter() == param;
            break;
        case ResourceType.Binding:
            auto binding = to!ParameterBinding(res);
            if (incArmedParameter() && incArmedParameter().bindings.canFind(binding)) {
                isActive = binding.isSet(incParamPoint());
            }
            break;
        default:
        }

        if (igSelectable("%s%s%s".format(noIcon? "": incTypeIdToIcon(res.typeId), isActive? "":"", res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 20))) {
            if (isNode) {
                node = to!Node(res);
                incSelectNode(node);
            }
        }
        bool isHovered = igIsItemHovered(); // Check if the selectable is hovered
        const char* popupName  = "##TreeViewNodeView";
        const char* popupName2 = "##TreeViewNodeMenu";
        if (isHovered && (isNode || res.type == ResourceType.Parameter)) {
            if (igIsItemClicked(ImGuiMouseButton.Right)) {
                igOpenPopup(popupName);
                if (isNode)
                    igOpenPopup(popupName2);
            }
        }
        if (igBeginPopup(popupName)) {
            if (res.uuid in contentsDrawn) return;
            showContents(res);
            contentsDrawn[res.uuid] = true;
            igEndPopup();
        }

        if (node !is null) {        
            bool isRoot = node.parent is null;
            auto flags = ImGuiWindowFlags.NoCollapse | ImGuiWindowFlags.NoDocking;

            if (isRoot)
                incNodeActionsPopup!(popupName2, true, true)(node);
            else
                incNodeActionsPopup!(popupName2, false, true)(node);
        }

        if (isHovered && igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
            if (panel) {
                string selectorStr = " %s#%d".format(res.typeId, res.uuid);
                panel.setCommand(selectorStr);
            }
        }
        if (res !in nodeIncluded) {
            igPopStyleColor();
        }
    }
    
public:
    this(CommandIssuer panel) {
        this.panel = panel;
    }

    override
    void onUpdate() {
        if (nodes.length > 0) {
            igPushID(cast(void*)this);
            contentsDrawn.clear();

            void traverse(Resource res) {
                ImGuiTreeNodeFlags flags = setFlag(res);
                bool opened = igTreeNodeEx(cast(void*)res.uuid, flags, "");

                drawTreeItem(res);

                if (opened) {
                    if (res in children) {
                        foreach (child; children[res])
                            traverse(child);
                    }
                    igTreePop();
                }
            }

            auto style = igGetStyle();
            auto spacing = style.IndentSpacing;
            style.IndentSpacing /= 2.5;

            foreach (r; roots) {
                traverse(r);
            }
            style.IndentSpacing = spacing;
            igPopID();
        }
    }

    void setResources(Resource[] nodes_) {
        nodes = nodes_;
        roots.length = 0;
        children.clear();
        nodeIncluded.clear();
        Resource[Resource] parentMap;
        bool[Resource] rootMap;
        bool[Resource][Resource] childMap;
        foreach (n; nodes) {
            nodeIncluded[n] = true;
        }

        void addToMap(Resource res, int level = 0) {
            if (res in parentMap) return;
            auto source = res.source;
            while (source) {
                if (source.source is null) break;
                if (source in nodeIncluded || source.explicit) break;
                source = source.source;
            }
            if (source) {
                parentMap[res] = source;
                childMap.require(source);
                childMap[source][res] = true;
                addToMap(source, level + 1);
            } else {
                rootMap[res] = true;
            }
        }

        foreach (res; nodes) {
            addToMap(res);
        }
        foreach (res; childMap.keys) {
            if (res !in parentMap || parentMap[res] is null) {
                rootMap[res] = true;
            }
        }
        roots = rootMap.keys.sort!((a,b)=>a.index<b.index).array;
        foreach (item; childMap.byKeyValue) {
            children[item.key] = item.value.keys.sort!((a,b)=>a.index<b.index).array;
        }
    }
}


class IconTreeOutput : TreeOutput {
protected:
    string grabParam;
    struct SubItemLayout {
        ImRect bounds;
        bool nextInHorizontal = false;
        bool scrolledOut = false;
    }
    SubItemLayout[uint] layout;
    bool prevMouseDown = false;
    bool mouseDown = false;

    override
    void drawTreeItem(Resource res) {
        bool selected = false;
        ImVec2 spacing, minPos;
        Node node;
        bool hovered = false;
        ImVec2 widgetMinPos, widgetMaxPos;
        igSameLine();
        switch (res.type) {
            case ResourceType.Node:
                node = to!Node(res);
                selected = incNodeInSelection(node);

                if (res !in nodeIncluded) {
                    setTransparency(0.5, 0.5);
                }

                if (node.typeId == "Part") {
                    spacing = igGetStyle().ItemSpacing;
                    igGetStyle().ItemSpacing = ImVec2(0, 0);
                    auto part = cast(Part)node;
                    if (igSelectable("###%s".format(res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, IconSize))) {
                        incSelectNode(node);
                    }
                    hovered = igIsItemHovered();
                    igSetItemAllowOverlap();
                    igSameLine();
                    igGetItemRectMin(&widgetMinPos);
                    igGetCursorPos(&minPos);
                    incTextureSlotUntitled("ICON", part.textures[0], ImVec2(IconSize, IconSize), 1, ImGuiWindowFlags.NoInputs);
                    igGetItemRectMax(&widgetMaxPos);
                    incTooltip(_(res.name));
                } else {
                    igGetItemRectMin(&widgetMinPos);
                    if (igSelectable("%s%s".format(incTypeIdToIcon(res.typeId), res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 20))) {
                        incSelectNode(node);
                    }
                    igGetItemRectMax(&widgetMaxPos);
                    hovered = igIsItemHovered();
                }

                if (res !in nodeIncluded) {
                    igPopStyleColor(6);
                }
                break;
            case ResourceType.Parameter:
                bool isActive = false;
                auto param = to!Parameter(res);
                isActive = incArmedParameter() == param;
                if (auto exGroup = cast(ExParameterGroup)param) {
                    igGetItemRectMin(&widgetMinPos);
                    if (igSelectable("%s%s".format(incTypeIdToIcon(res.typeId), res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 20))) {
                    }
                    hovered = igIsItemHovered();
                    igGetItemRectMax(&widgetMaxPos);
                } else {
                    igGetItemRectMin(&widgetMinPos);
                    igPushID(cast(void*)param);
                    if (incController!(4, 4, 3)("###CONTROLLER", param, ImVec2(IconSize, IconSize), false, grabParam)) {
                        if (igIsMouseDown(ImGuiMouseButton.Left)) {
                            if (grabParam == null)
                                grabParam = param.name;
                        } else {
                            grabParam = "";
                        }
                    }
                    hovered = igIsItemHovered();
                    igGetItemRectMax(&widgetMaxPos);
                    incTooltip(_(res.name));
                    igPopID();
                }
                break;
            case ResourceType.Binding:
                bool isActive = false;
                auto binding = to!ParameterBinding(res);
                if (incArmedParameter() && incArmedParameter().bindings.canFind(binding)) {
                    isActive = binding.isSet(incParamPoint());
                }
                break;
            default:
                igGetItemRectMin(&widgetMinPos);
                igGetItemRectMax(&widgetMaxPos);
        }

        bool isHovered = igIsItemHovered(); // Check if the selectable is hovered
        const char* popupName  = "##IconTreeViewNodeView";
        const char* popupName2 = "##IconTreeViewNodeMenu";
        bool menuOpened = isHovered && 
            (res.type == ResourceType.Node || res.type == ResourceType.Parameter) &&
            igIsItemClicked(ImGuiMouseButton.Right);

        if (popupOpened == res) {
            auto flags = ImGuiWindowFlags.NoCollapse | 
                        ImGuiWindowFlags.NoDocking |
                        ImGuiWindowFlags.AlwaysAutoResize | 
                        ImGuiWindowFlags.NoTitleBar | 
                        ImGuiWindowFlags.NoSavedSettings;
            if (igBegin(popupName, null, flags)) {
                hovered |= popupOpened == res && isWindowHovered();
                if (res.uuid in contentsDrawn) return;
                showContents(res);
                contentsDrawn[res.uuid] = true;

                auto window = igFindWindowByName(popupName);
                adjustWindowPos!true(widgetMinPos, widgetMaxPos, window);
            }
            igEnd();

            if (node !is null && popupOpened) {
                bool isRoot = node.parent is null;

                if (igBegin(popupName2, null, flags)) {
                    hovered |= popupOpened == res && isWindowHovered();
                    if (isRoot)
                        incNodeActionsPopup!(null, true, true)(node);
                    else
                        incNodeActionsPopup!(null, false, true)(node);

                    auto window = igFindWindowByName(popupName2);
                    adjustWindowPos!false(widgetMinPos, widgetMaxPos, window);
                }
                igEnd();
            }
            if (!hovered && mouseDown && !prevMouseDown) {
                popupOpened = null;
                igGetIO().MouseClicked[ImGuiMouseButton.Left] = false;
                igGetIO().MouseClicked[ImGuiMouseButton.Right] = false;
            }
        }
        if (menuOpened && popupOpened is null)
            popupOpened = res;
        if (isHovered && igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
            if (res.uuid !in layout) {
                layout.require(res.uuid);
                layout[res.uuid].nextInHorizontal = true;
            }else {
                layout[res.uuid].nextInHorizontal = ! layout[res.uuid].nextInHorizontal;
            }
        }
        if (res.type == ResourceType.Node) {
            if (node.typeId == "Part") {
                igGetStyle().ItemSpacing = spacing;
            }
        }
    }
public:
    this(CommandIssuer panel) {
        super(panel);
        IconSize = 64;
    }

    override
    void onUpdate() {
        if (nodes.length > 0) {
            mouseDown = igIsAnyMouseDown();
            igPushID(cast(void*)this);
            contentsDrawn.clear();

            ImRect traverse(Resource res, ref bool prevHorz, ref uint[] parentUUIDs) {
                ImGuiTreeNodeFlags flags = setFlag(res);
                bool nextInHorizontal = res.uuid in layout && layout[res.uuid].nextInHorizontal;
                if (nextInHorizontal || prevHorz) {
                    if (prevHorz) {
                        igEndChild();
                        igSameLine();
                        parentUUIDs = parentUUIDs.remove(parentUUIDs.length - 1);
                    }
                    layout.require(res.uuid);
                    auto itemLayout = layout[res.uuid];
                    igBeginChild("##horz%d".format(res.uuid).toStringz, ImVec2(max(IconSize * 1.5, 
                                 itemLayout.bounds.Max.x - itemLayout.bounds.Min.x), itemLayout.bounds.Max.y - itemLayout.bounds.Min.y), 
                                 false, ImGuiWindowFlags.NoScrollbar|ImGuiWindowFlags.NoScrollWithMouse);
                    parentUUIDs ~= res.uuid;
                }
                bool opened = igTreeNodeEx(cast(void*)res.uuid, flags, "");

                drawTreeItem(res);
                ImRect result;
                igGetItemRectMin(&result.Min);
                igGetItemRectMax(&result.Max);
                if (result.Max.x > result.Min.x + IconSize * 1.5) {
                    result.Max.x = result.Min.x + IconSize * 1.5;
                }

                if (opened) {
                    if (res in children) {
                        bool subHorz = false;
                        uint[] subParentUUIDs = parentUUIDs[];
                        foreach (child; children[res]) {
                            ImRect subRect = traverse(child, subHorz, subParentUUIDs);
                            result.Min.x = min(result.Min.x, subRect.Min.x);
                            result.Min.y = min(result.Min.y, subRect.Min.y);
                            result.Max.x = max(result.Max.x, subRect.Max.x);
                            result.Max.y = max(result.Max.y, subRect.Max.y);
                        }
                        foreach (i; 0..(subParentUUIDs.length - parentUUIDs.length)) {
                            igEndChild();
                        }
                    }
                    igTreePop();
                }
                if (nextInHorizontal || prevHorz) {
                    layout.require(res.uuid);
                    if (result.Min.y != result.Max.y) {
                        layout[res.uuid].bounds = result;
                        layout[res.uuid].scrolledOut = false;
                    } else {
                        layout[res.uuid].scrolledOut = true;
                    }
                }
                foreach (parentUUID; parentUUIDs) {
                    if (parentUUID != InInvalidUUID) {
                        layout.require(parentUUID);
                        if (!layout[parentUUID].scrolledOut) {
                            ImRect bounds = layout[parentUUID].bounds;
                            bounds.Min.x = min(result.Min.x, bounds.Min.x);
                            bounds.Min.y = min(result.Min.y, bounds.Min.y);
                            bounds.Max.x = max(result.Max.x, bounds.Max.x);
                            bounds.Max.y = max(result.Max.y, bounds.Max.y);
                            layout[parentUUID].bounds = bounds;
                        }
                    }
                }
                if (nextInHorizontal) {
                    prevHorz = true;
                } else {
                    prevHorz = false;
                }
                return result;
            }

            auto style = igGetStyle();
            auto spacing = style.IndentSpacing;
            style.IndentSpacing /= 2.5;

            foreach (r; roots) {
                bool prevHorz = false;
                uint[] parentUUIDs;
                traverse(r, prevHorz, parentUUIDs);
                foreach (i; 0..parentUUIDs.length) {
                    igEndChild();
                }
            }

            foreach (k; layout.keys) {
                Resource node;
                foreach (r; nodes) {
                    if (r.uuid == k) {
                        node = r;
                        break;
                    }
                }
            }
            style.IndentSpacing = spacing;
            igPopID();
            prevMouseDown = mouseDown;
        }
    }

    override
    void setResources(Resource[] resources) {
        super.setResources(resources);
        popupOpened = null;
    }

}


class ViewOutput : Output {
protected:
    CommandIssuer panel;

    Resource[] nodes;
    Resource[][Resource] children;
    Resource[] roots;
    bool[uint] pinned;
    bool[Resource] nodeIncluded;
    Resource focused = null;
    string parameterGrabStr;
public:
    this(CommandIssuer panel) {
        this.panel = panel;
    }

    override
    void onUpdate() {
        bool[uint] contentsDrawn;
        ulong[] removed;
        foreach (i, res; nodes) {
            void showContents(Resource res) {
                if (res.uuid in contentsDrawn) return;
                ImVec2 size = ImVec2(20, 20);
                igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
                igPushStyleVar(ImGuiStyleVar.ItemInnerSpacing, ImVec2(0, 0));
                igPushStyleVar(ImGuiStyleVar.FrameBorderSize, 0.0f);
                if (igButton("\ue5cd", size)) {                    
                    removed ~= i;
                }

                if (res.type == ResourceType.Node) {
                    igSameLine();
                    igText(res.name.toStringz);
                    Node node = to!Node(res);
                    onNodeView(node);
                } else if (res.type == ResourceType.Parameter) {
                    Parameter param = to!Parameter(res);
                    igSameLine();
                    incParameterViewEditButtons!(false, true)(res.index, param, incActivePuppet.parameters, true);
                    igSameLine();
                    igText(res.name.toStringz);
                    onParameterView(res.index, param);
                }
                igPopStyleVar(3);
                contentsDrawn[res.uuid] = true;
            };

            const char* popupName = "###%x".format(res.uuid).toStringz;
            setTransparency(focused == res? 0.8: 0.1, focused == res? 0.8: 0.3);
            ImGuiWindowFlags popupFlags;
            popupFlags = ImGuiWindowFlags.NoTitleBar;
            if (res.type == ResourceType.Parameter) {
                popupFlags |= ImGuiWindowFlags.NoResize;
            }
            if (igBegin(popupName, null, popupFlags)) {
                ImVec2 cursorPos;
                igGetMousePos(&cursorPos);
                ImVec2 windowPos;
                igGetWindowPos(&windowPos);
                ImVec2 windowSize;
                if (res.type == ResourceType.Node) {
                    windowSize.x = NodeViewWidth + 20;
                    igSetWindowSize(windowSize);
                } else if (res.type == ResourceType.Parameter) {
                    Parameter param = (cast(Proxy!Parameter)res).obj;
                    windowSize = ImVec2(166, param.isVec2? 186: 92);
                    igSetWindowSize(windowSize);
                }
                igGetWindowSize(&windowSize);

                ImVec2 windowEndPos = ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
                bool isCursorInside = (cursorPos.x >= windowPos.x && cursorPos.x <= windowEndPos.x) &&
                                    (cursorPos.y >= windowPos.y && cursorPos.y <= windowEndPos.y);
                
                if (isCursorInside) {
                    focused = res;
                } else if (focused == res) {
                    focused = null;
                }

                showContents(res);
                igEnd();
            }
            igPopStyleColor(6);
        }
        foreach_reverse (r; removed) {
            nodes = nodes.remove(r);
        }
    }

    void addResources(Resource[] nodes_) {
        nodes ~= nodes_;
    }
}