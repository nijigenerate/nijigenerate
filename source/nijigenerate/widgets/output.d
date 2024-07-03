module nijigenerate.widgets.output;

import std.stdio;
import std.array;
import std.string;
import std.math;
import std.algorithm;
import i18n;
import nijilive;
import nijigenerate;
import nijigenerate.actions;
import nijigenerate.core;
import nijigenerate.core.selector;
import nijigenerate.core.selector.resource: Resource, ResourceInfo, ResourceType;
import nijigenerate.panels;
import nijigenerate.utils;
import nijigenerate.widgets;
import nijigenerate.panels.inspector;
import nijigenerate.panels.parameters;
import nijigenerate.panels.nodes;
import nijigenerate.ext;

private {
    string parameterGrabStr;
    enum NodeViewWidth = 250;
    enum PopupSide {
        HorzLarger,
        HorzNarrower,
        VertLarger,
        VertNarrower
    };

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

    // This code rlies on imgui_internal.h
    // getting window rectangle for specified window.
    // igGetPopupAllowedExtentRect is used in the implementation of Popup window.
    ImRect getOuterRect(ImGuiWindow* window) {
        ImRect r_outer;
        igGetPopupAllowedExtentRect(&r_outer, window);
        return r_outer;
    }

    // This code relies on imgui_internal.h
    // Adjusting window position requires detailed internal information in ImGuiWindow.
    void adjustWindowPos(PopupSide side)(ImVec2 minPos, ImVec2 maxPos, ImGuiWindow* window) {
        if (window is null || window.RootWindow is null) return;
        ImRect r_outer = getOuterRect(window);
        float spaceLeft = minPos.x - r_outer.Min.x;
        float spaceRight = r_outer.Max.x - r_outer.Min.x - maxPos.x;
        float spaceUp = minPos.y - r_outer.Min.y;
        float spaceDown = r_outer.Max.y - r_outer.Min.y - minPos.y;

        switch (side) {
            case PopupSide.HorzLarger:
                if (spaceLeft > spaceRight) {
                    igSetWindowPos(window, ImVec2(minPos.x - window.Size.x, minPos.y));
                } else {
                    igSetWindowPos(window, ImVec2(maxPos.x, minPos.y));
                }
                break;
            case PopupSide.HorzNarrower:
                if (spaceLeft > spaceRight) {
                    igSetWindowPos(window, ImVec2(maxPos.x, minPos.y));
                } else {
                    igSetWindowPos(window, ImVec2(minPos.x - window.Size.x, minPos.y));
                }
                break;
            case PopupSide.VertLarger:
                if (spaceUp > spaceDown) {
                    igSetWindowPos(window, ImVec2(minPos.x, minPos.y - window.Size.y));
                } else {
                    igSetWindowPos(window, ImVec2(minPos.x, maxPos.y));
                }
                break;
            case PopupSide.VertNarrower:
                if (spaceUp > spaceDown) {
                    igSetWindowPos(window, ImVec2(minPos.x, maxPos.y));
                } else {
                    igSetWindowPos(window, ImVec2(minPos.x, minPos.y - window.Size.y));
                }
                break;
            default:
                break;
        }
        if (side == PopupSide.HorzLarger || side == PopupSide.HorzNarrower) {
            if (window.Size.y >= spaceDown) {
                igSetWindowPos(window, ImVec2(window.Pos.x, window.Pos.y - (window.Size.y - spaceDown)));
            }
            if (window.Pos.y < 0) {
                igSetWindowPos(window, ImVec2(window.Pos.x, 0));
            }
        } else {
            if (window.Size.x >= spaceRight) {
                igSetWindowPos(window, ImVec2(window.Pos.x - (window.Size.x - spaceRight), window.Pos.y));
            }
            if (window.Pos.x < 0) {
                igSetWindowPos(window, ImVec2(0, window.Pos.y));
            }
        }
    }

    string[string] transformName;
    static this() {
        transformName = [
            "transform.t.x": "\ue89f-X",
            "transform.t.y": "\ue89f-Y",
            "transform.t.z": "\ue89f-Z",
            "transform.s.x": "\ue8ff-X",
            "transform.s.y": "\ue8ff-Y",
            "transform.s.z": "\ue8ff-Z",
            "transform.r.x": "\ue863-X",
            "transform.r.y": "\ue863-Y",
            "transform.r.z": "\ue863-Z",
            "deform"       : "\ue3ea",
        ];
    }


    string getTransformText(string name) {
        if (name in transformName) {
            return transformName[name];
        } else
            return name;
    }
}

interface CommandIssuer {
    void addCommand(string);
    void setCommand(string);
    void addPopup(Resource);
}

interface Output {
    void onUpdate();
    void reset();
}

class TreeStore {
public:
    CommandIssuer panel;

    Resource[] nodes;
    Resource[][Resource] children;
    Resource[] roots;
    bool[Resource] nodeIncluded;

    void reset() {
        children.clear();
        nodes.length = 0;
        nodeIncluded.clear();
        roots.length = 0;
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

class ListOutput : Output {
protected:
    TreeStore self;

    int IconSize = 20;
    ref CommandIssuer panel() { return self.panel; }
    Resource popupOpened = null;

    ref Resource[] nodes() { return self.nodes; }
    ref Resource[][Resource] children() { return self.children; }
    ref Resource[] roots() { return self.roots; }
    ref bool[Resource] nodeIncluded() { return self.nodeIncluded; }
    Resource focused = null;
    bool[uint] contentsDrawn;
    ParamDragDropData* dragDropData;

    void setNodeReorderDragTarget(Resource res, bool horizontal = true, float width = -1) {
        if (res.type == ResourceType.Node) {
            auto selectedNodes = incSelectedNodes();
            ImVec2 avail = incAvailableSpace();
            width = horizontal? (width < 0? IconSize * 1.5: width): 4;
            float height = horizontal? 4: max(avail.y, IconSize);
            auto paddingY = igGetStyle().FramePadding.y;
            igGetStyle().FramePadding.y = 0;
            igInvisibleButton("###TARGET%d".format(res.uuid).toStringz, ImVec2(width, height));
            igGetStyle().FramePadding.y = paddingY;

            Node node = to!Node(res);
            if(igBeginDragDropTarget()) {
                const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
                if (payload !is null) {
                    Node payloadNode = *cast(Node*)payload.Data;
                    auto index = node.parent.children.countUntil(node);
                    
                    try {
                        if (selectedNodes.length > 1) incMoveChildrenWithHistory(selectedNodes, node.parent, index);
                        else incMoveChildWithHistory(payloadNode, node.parent, index);
                    } catch (Exception ex) {
                        incDialog(__("Error"), ex.msg);
                    }
                }
                igEndDragDropTarget();
            }
        }
    }

    void setNodeDragSource(Node node) {
        bool isRoot = node.parent is null;
        auto selectedNodes = incSelectedNodes();
        if (!isRoot) {
            if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                igSetDragDropPayload("_PUPPETNTREE", cast(void*)&node, (&node).sizeof, ImGuiCond.Always);
                if (selectedNodes.length > 1) {
                    incDragdropNodeList(selectedNodes);
                } else {
                    incDragdropNodeList(node);
                }
                igEndDragDropSource();
            }
        }
    }
    void setNodeReparentDragTarget(Node node) {
        if(igBeginDragDropTarget()) {
            auto selectedNodes = incSelectedNodes();
            const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
            if (payload !is null) {
                Node payloadNode = *cast(Node*)payload.Data;
                
                try {
                    if (selectedNodes.length > 1) incMoveChildrenWithHistory(selectedNodes, node, 0);
                    else incMoveChildWithHistory(payloadNode, node, 0);
                } catch (Exception ex) {
                    incDialog(__("Error"), ex.msg);
                }
            }
            igEndDragDropTarget();
        }
    }

    void setParamDragSource(Parameter param) {
        if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
            if (!dragDropData) dragDropData = new ParamDragDropData;
            
            dragDropData.param = param;

            igSetDragDropPayload("_PARAMETER", cast(void*)&dragDropData, (&dragDropData).sizeof, ImGuiCond.Always);
            incText(dragDropData.param.name);
            igEndDragDropSource();
        }        
    }

    void setParamDragTarget(ExParameterGroup group) {
        incBeginDragDropFake();
            auto peek = igAcceptDragDropPayload("_PARAMETER", ImGuiDragDropFlags.AcceptPeekOnly | ImGuiDragDropFlags.SourceAllowNullID);
            if(peek && peek.Data) {
                if (igBeginDragDropTarget()) {
                    auto payload = igAcceptDragDropPayload("_PARAMETER");
                    
                    if (payload !is null) {
                        ParamDragDropData* payloadParam = *cast(ParamDragDropData**)payload.Data;
                        incMoveParameter(payloadParam.param, group);
                        auto root = incActivePuppet().root;
                        root.notifyChange(root, NotifyReason.StructureChanged);
                    }
                    igEndDragDropTarget();
                }
            }
        incEndDragDropFake();

    }

    // Show content view. (determined based on resource type.)
    // This is called in popu window opened by right click.
    void showContents(Resource res) {
        ImVec2 size = ImVec2(20, 20);
        void showPinning() {
            if (incButtonColored("\ue763", size)) {
                if (panel) {
                    panel.addPopup(res);
                    popupOpened = null;
                }
            }
        }

        if (res.type == ResourceType.Node) {
            showPinning();
            Node node = to!Node(res);
            igSameLine();
            if (incButtonColored("\ue8b8", size)) {
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
            if (auto exGroup = cast(ExParameterGroup)param) {
                if (exGroup.children.length == 0) {
                    if (igMenuItem(__("New Parameter Group"), "", false, true)) {
                        ExPuppet puppet = cast(ExPuppet)incActivePuppet();
                        incCreateParamGroup(puppet? cast(int)puppet.groups.length: 0);
                        auto root = incActivePuppet().root;
                        root.notifyChange(root, NotifyReason.StructureChanged);
                    }
                    igSeparator();
                    incParameterMenuContents(incActivePuppet().parameters);
                } else {
                    incParameterGropuMenuContents(exGroup);
                }
            } else {
                showPinning();
                igSameLine();
                incParameterViewEditButtons!(false, true)(res.index, param, incActivePuppet.parameters, true);
                igSameLine();
                igText(res.name.toStringz);
                onParameterView(res.index, param);
            }
        } else if (res.type == ResourceType.Binding) {
            auto binding = to!ParameterBinding(res);
            auto vimpl = cast(ValueParameterBinding)binding;
            auto dimpl = cast(DeformationParameterBinding)binding;
            ParameterBinding[BindTarget] bindings;
            bindings[binding.getTarget()] = binding;
            if (vimpl)
                incBindingMenuContents(vimpl.parameter, bindings);
            else if (dimpl)
                incBindingMenuContents(dimpl.parameter, bindings);
        }

    };

    ImGuiTreeNodeFlags setFlag(Resource res) {
        ImGuiTreeNodeFlags flags;
        if (res !in children) 
            flags |= ImGuiTreeNodeFlags.Leaf;
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
        if (isNode) {
            setNodeDragSource(node);
            setNodeReparentDragTarget(node);
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
    this(TreeStore self, CommandIssuer panel) {
        this.self = self;
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

                igPushID(cast(void*)res.uuid);
                drawTreeItem(res);
                igPopID();

                if (opened) {
                    if (res in children) {
                        foreach (child; children[res]) {
                            setNodeReorderDragTarget(child, true, 128);
                            traverse(child);
                        }
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
        self.setResources(nodes_);
    }

    void reset() {
        focused = null;
        contentsDrawn.clear();
        self.reset();
    }

}


class IconTreeOutput : ListOutput {
protected:
    string grabParam;
    struct SubItemLayout {
        ImRect bounds;
        bool nextInHorizontal = false;
        bool scrolledOut = false;
    }
    SubItemLayout[uint] _layout;
    bool prevMouseDown = false;
    bool mouseDown = false;
    bool showRootThumb = false;
    Snapshot snapshot = null;

    void drawNode(Resource res, Node node, ref ImVec2 widgetMinPos, ref ImVec2 widgetMaxPos, ref bool hovered) {
        bool selected = incNodeInSelection(node);
        bool isRoot = node.parent is null || node == incActivePuppet().root;
        auto spacing = igGetStyle().ItemSpacing;

        if (res !in nodeIncluded) {
            setTransparency(0.5, 0.5);
        }

        void onSelect(Node n) {
            auto io = igGetIO();
            if (selected) {
                if (incSelectedNodes().length > 1) {
                    if (io.KeyCtrl) incRemoveSelectNode(n);
                    else incSelectNode(n);
                }
            } else {
                if (io.KeyCtrl) incAddSelectNode(n);
                else incSelectNode(n);
            }            if (igGetIO().KeyCtrl) {}
        }

        if (isRoot) {
            // Root node.
            // Show thumbnail if specified. Show normal tree otherwise.
            if (showRootThumb) {
                // Show thumbnail.
                spacing = igGetStyle().ItemSpacing;
                igGetStyle().ItemSpacing = ImVec2(0, 0);
                auto part = cast(Part)node;
                if (igSelectable("###%s".format(res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, IconSize * 3))) {
                    onSelect(node);
                }
                hovered = igIsItemHovered();
                igSetItemAllowOverlap();
                igSameLine();
                igGetItemRectMin(&widgetMinPos);
                auto paddingY = igGetStyle().FramePadding.y;
                igGetStyle().FramePadding.y = 1;
                if (snapshot is null)
                    snapshot = Snapshot.get(incActivePuppet());
                incTextureSlotUntitled("ICON", snapshot.capture(), ImVec2(IconSize * 3, IconSize * 3), 1, ImGuiWindowFlags.NoInputs, selected);
                igGetStyle().FramePadding.y = paddingY;
                if (igIsItemClicked()) {
                    onSelect(node);
                }
                igGetItemRectMax(&widgetMaxPos);
                incTooltip(_(res.name));
            } else {
                // Show normal tree node.
                igGetItemRectMin(&widgetMinPos);
                if (igSelectable("%s%s".format(incTypeIdToIcon(res.typeId), res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 20))) {
                    onSelect(node);
                }
                igGetItemRectMax(&widgetMaxPos);
                hovered = igIsItemHovered();
            }
        } else if (node.typeId == "Part") {
            // Part node.
            // Show thumbnail of the Part image.
            spacing = igGetStyle().ItemSpacing;
            igGetStyle().ItemSpacing = ImVec2(0, 0);
            auto part = cast(Part)node;
            if (igSelectable("###%s".format(res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, IconSize))) {
                onSelect(node);
            }
            hovered = igIsItemHovered();
            igSetItemAllowOverlap();
            igSameLine();
            igGetItemRectMin(&widgetMinPos);
            auto paddingY = igGetStyle().FramePadding.y;
            igGetStyle().FramePadding.y = 1;
            incTextureSlotUntitled("ICON", part.textures[0], ImVec2(IconSize, IconSize), 1, ImGuiWindowFlags.NoInputs, selected);
            igGetStyle().FramePadding.y = paddingY;
            if (igIsItemClicked()) {
                onSelect(node);
            }
            igGetItemRectMax(&widgetMaxPos);
            incTooltip(_(res.name));
        } else {
            // Node other than Part object.
            // Show icon and name.
            igGetItemRectMin(&widgetMinPos);
            if (igSelectable("%s%s".format(incTypeIdToIcon(res.typeId), res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 20))) {
                onSelect(node);
            }
            igGetItemRectMax(&widgetMaxPos);
            hovered = igIsItemHovered();
        }

        if (res !in nodeIncluded) {
            igPopStyleColor(6);
        }
        setNodeDragSource(node);
        setNodeReparentDragTarget(node);
        if (isRoot || node.typeId == "Part")
            igGetStyle().ItemSpacing = spacing;
    }

    bool drawParameter(Resource res, ref ImVec2 widgetMinPos, ref ImVec2 widgetMaxPos, ref bool hovered) {
        bool isActive = false;
        auto param = to!Parameter(res);
        isActive = incArmedParameter() == param;
        bool menuOpened = false;
        if (auto exGroup = cast(ExParameterGroup)param) {
            igGetItemRectMin(&widgetMinPos);
            if (igSelectable("%s%s".format(incTypeIdToIcon(res.typeId), res.name).toStringz, false, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 20))) {
            }
            setParamDragTarget(exGroup);
            igGetItemRectMax(&widgetMaxPos);
        } else {
            igGetItemRectMin(&widgetMinPos);
            igPushID(cast(void*)param);
            if (igSelectable(res.name.toStringz, incArmedParameter() == param, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 16))) {
            }
            if (igIsItemClicked(ImGuiMouseButton.Right)) {
                menuOpened = true;
            }
            setParamDragSource(param);
            if (incController!(4, 4, 3, 1)("###CONTROLLER", param, ImVec2(IconSize, IconSize), false, grabParam)) {
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
        return menuOpened;
    }

    void drawBinding(Resource res, ref ImVec2 widgetMinPos, ref ImVec2 widgetMaxPos, ref bool hovered) {
        bool isActive = false;
        auto binding = to!ParameterBinding(res);
        if (incArmedParameter() && incArmedParameter().bindings.canFind(binding)) {
            isActive = binding.isSet(incParamPoint());
        }
        igGetItemRectMin(&widgetMinPos);
        if (igSelectable("%s%s%s".format(isActive? "\ue5de": "", getTransformText(res.name), isActive? "\ue5df": "").toStringz, false, ImGuiSelectableFlags.None, ImVec2(0, 16))) {
        }
        hovered = igIsItemHovered();
        igGetItemRectMax(&widgetMaxPos);
    }

    override
    void drawTreeItem(Resource res) {
        Node node;
        bool hovered = false;
        ImVec2 widgetMinPos, widgetMaxPos;
        igSameLine();
        // Draw contents of TreeNodeEx
        bool menuOpened = false;
        switch (res.type) {
            case ResourceType.Node:
                node = to!Node(res);
                drawNode(res, node, widgetMinPos, widgetMaxPos, hovered);
                break;

            case ResourceType.Parameter:
                menuOpened = drawParameter(res, widgetMinPos, widgetMaxPos, hovered);
                break;

            case ResourceType.Binding:
                drawBinding(res, widgetMinPos, widgetMaxPos, hovered);
                break;

            default:
                igGetItemRectMin(&widgetMinPos);
                igGetItemRectMax(&widgetMaxPos);
        }

        // Code for pseudo popup menu
        bool isHovered = igIsItemHovered(); // Check if the selectable is hovered
        const char* popupName  = "##IconTreeViewNodeView";
        const char* popupName2 = "##IconTreeViewNodeMenu";
        menuOpened = menuOpened || (isHovered && igIsItemClicked(ImGuiMouseButton.Right));

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

                // This code relies on imgui_internal.h, to obtain ImGuiWindow.
                auto window = igFindWindowByName(popupName);
                adjustWindowPos!(PopupSide.HorzLarger)(widgetMinPos, widgetMaxPos, window);
            }
            igEnd();

            if (node !is null && popupOpened) {
                bool isRoot = node.parent is null || node == incActivePuppet().root;

                if (igBegin(popupName2, null, flags)) {
                    hovered |= popupOpened == res && isWindowHovered();
                    if (isRoot) {
                        incNodeActionsPopup!(null, true, true)(node);
                        if (igMenuItem(__("\ue84e"), null, false, true)) {
                            showRootThumb = !showRootThumb;
                            if (!showRootThumb) {
                                if (snapshot) {
                                    snapshot.release();
                                    snapshot = null;
                                }
                            }
                        }
                    } else {
                        incNodeActionsPopup!(null, false, true)(node);
                    }

                    // This code relies on imgui_internal.h, to obtain ImGuiWindow.
                    auto window = igFindWindowByName(popupName2);
                    adjustWindowPos!(PopupSide.VertLarger)(widgetMinPos, widgetMaxPos, window);
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

        // Toggle direction of next tree item
        if (isHovered && igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
            if (res.uuid !in layout) {
                layout.require(res.uuid);
                layout[res.uuid].nextInHorizontal = true;
            }else {
                layout[res.uuid].nextInHorizontal = ! layout[res.uuid].nextInHorizontal;
            }
        }
    }
public:
    this(TreeStore self, CommandIssuer panel) {
        super(self, panel);
        IconSize = 48;
    }

    override
    void onUpdate() {
        if (nodes.length > 0) {
            mouseDown = igIsAnyMouseDown();
            igPushID(cast(void*)this);
            contentsDrawn.clear();

            bool popupVisited = popupOpened is null;

            ImRect traverse(Resource res, ref bool prevHorz, ref uint[] parentUUIDs) {
                if (popupOpened == res) popupVisited = true;
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

                igPushID(cast(void*)res.uuid);
                drawTreeItem(res);
                igPopID();
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
                            setNodeReorderDragTarget(child, !subHorz);

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

            if (!popupVisited) {
                popupOpened = null;
                mouseDown = false;
                prevMouseDown = false;
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
        mouseDown = false;
        prevMouseDown = false;
    }

    ref SubItemLayout[uint] layout() {
        return this._layout;
    }

    override
    void reset() {
        layout.clear();
        prevMouseDown = false;
        mouseDown = false;
        popupOpened = null;
        if (snapshot)
            snapshot.release();
        snapshot = null;
        super.reset();
    }

}


class ViewOutput : Output {
protected:
    CommandIssuer panel;

    Resource[] nodes;
    Resource focused = null;
    string parameterGrabStr;
public:
    this(CommandIssuer panel) {
        this.panel = panel;
    }

    override
    void onUpdate() {
        if (incEditMode() == EditMode.VertexEdit) return;
        bool[uint] contentsDrawn;
        ulong[] removed;
        foreach (i, res; nodes) {
            void showContents(Resource res) {
                if (res.uuid in contentsDrawn) return;
                ImVec2 size = ImVec2(20, 20);
                igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
                igPushStyleVar(ImGuiStyleVar.ItemInnerSpacing, ImVec2(0, 0));
                igPushStyleVar(ImGuiStyleVar.FrameBorderSize, 0.0f);
                if (incButtonColored("\ue5cd", size)) {                    
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

    void refresh(Resource[] resources) {
        Resource[] newRes;
        foreach (node; nodes) {
            foreach (res; resources) {
                if (res.uuid == node.uuid) {
                    newRes ~= res;
                }
            }
        }
        nodes = newRes;
    }

    void addResources(Resource[] nodes_) {
        nodes ~= nodes_;
    }

    void reset() {
        nodes.length = 0;
        focused = null;
    }
}
