/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.nodes;
import nijigenerate.viewport.vertex;
import nijigenerate.widgets.dragdrop;
import nijigenerate.widgets.tooltip;
import nijigenerate.actions;
import nijigenerate.core.actionstack;
import nijigenerate.panels;
import nijigenerate.ext;
import nijigenerate.utils.transform;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.ext;
import nijigenerate.core;
import nijigenerate.core.input;
import nijigenerate.utils;
import nijilive;
import std.algorithm;
import std.string;
import std.array;
import std.format;
import std.conv;
import std.utf;
import i18n;
import nijigenerate.commands;
import nijigenerate.commands.node.dynamic : ensureAddNodeCommand, ensureInsertNodeCommand;
import nijigenerate.commands.node.base;

private {
    // Follow selection toggle (panel-local, default disabled)
    bool followSelection = false;
    string[string] actionIconMap;
    string suffixName;
    static this() {
        actionIconMap = [
            "Add": "\ue145",
            "Insert": "\ue15e",
            "Edit Mesh": "\ue3c9",
            "Delete": "\ue872",
            "Show": "\ue8f4",
            "Hide": "\ue8f5",
            "Copy": "\ue14d",
            "Paste": "\ue14f",
            "Reload": "\ue5d5",
            "More Info": "\ue88e",
            "Parameter Usage": "\ue429",
            "Recalculate origin": "\ue57b",
            "Convert To...": "\ue043",
        ];
    }

    string nodeActionToIcon(bool icon)(string name) {
        if (icon) {
            if (name in actionIconMap) {
                return actionIconMap[name];
            }
            return "--";
        } else {
            return name;
        }
    }
}

void ngAddOrInsertNodeMenu(bool add)() {
    void insertNode(void function(Node[], string, string) callback_unused) {
        auto NodeCreateMenu(string NodeName, string NodeLabel = null, string ClassName = null) {
            if (ClassName is null) {
                ClassName = NodeName;
            }
            if (NodeLabel is null) {
                NodeLabel = _(NodeName);
            }

            incText(incTypeIdToIcon(NodeName));
            igSameLine(0, 2);

            if (igMenuItem(NodeLabel.toStringz, null, false, true)) {
                Context ctx = new Context();
                ctx.puppet = incActivePuppet();
                auto nodes = incSelectedNodes();
                if (nodes.length > 0) ctx.nodes = nodes;
                static if (add) {
                    auto cmd = ensureAddNodeCommand(ClassName, suffixName);
                    cmd.run(ctx);
                } else {
                    auto cmd = ensureInsertNodeCommand(ClassName, suffixName);
                    cmd.run(ctx);
                }
                suffixName = null;
            }
        }

        incText(_("Suffix"));
        igSameLine();
        if (incInputText("###NAME_SUFFIX", incAvailableSpace().x-24, suffixName)) {
            try {
                suffixName = suffixName.toStringz.fromStringz;
            } catch (std.utf.UTFException e) {}
        }
        NodeCreateMenu("Node", _("Node"));
        NodeCreateMenu("Mask", _("Mask"));
        NodeCreateMenu("Composite", _("Composite"));
        NodeCreateMenu("SimplePhysics", _("Simple Physics"));
        NodeCreateMenu("MeshGroup", _("Mesh Group"));
        NodeCreateMenu("DynamicComposite", _("Dynamic Composite"));
        NodeCreateMenu("PathDeformer", _("Path Deformer"));
        NodeCreateMenu("GridDeformer", _("Grid Deformer"));
        NodeCreateMenu("Camera", _("Camera"));
    }

    static if (add) {
        insertNode(null);
    } else {
        insertNode(null);
    }
}
alias ngAddNodeMenu = ngAddOrInsertNodeMenu!true;
alias ngInsertNodeMenu = ngAddOrInsertNodeMenu!false;

void incNodeActionsPopup(const char* title, bool isRoot = false, bool icon = false)(Node n) {
    if (title == null || igBeginPopup(title)) {
        Context ctx = new Context();
        ctx.puppet = incActivePuppet();
        ctx.nodes = [n];
        
        auto selected = incSelectedNodes();
        
        if (igBeginMenu(__(nodeActionToIcon!icon("Add")), true)) {
            ngAddNodeMenu();
            igEndMenu();
        }
        if (icon) incTooltip(_(nodeActionToIcon!false("Add")));

        if (igBeginMenu(__(nodeActionToIcon!icon("Insert")), true)) {
            ngInsertNodeMenu();
            igEndMenu();
        }
        if (icon) incTooltip(_(nodeActionToIcon!false("Insert")));

        static if (!isRoot) {

            // Edit mesh option for drawables
            if (auto d = cast(Deformable)n) {
                if (!incArmedParameter()) {
                    if (igMenuItem(__(nodeActionToIcon!icon("Edit Mesh")), "", false, true)) {
                        cmd!(NodeCommand.VertexMode)(ctx);
                    }
                }
            }
            if (icon) incTooltip(_(nodeActionToIcon!false("Edit Mesh")));
            
            if (igMenuItem(n.getEnabled() ? __(nodeActionToIcon!icon("Hide")) : __(nodeActionToIcon!icon("Show")))) {
                cmd!(NodeCommand.ToggleVisibility)(ctx);
            }
            if (icon) incTooltip(n.getEnabled()? _(nodeActionToIcon!false("Hide")) : _(nodeActionToIcon!false("Show")));

            if (igMenuItem(__(nodeActionToIcon!icon("Delete")), "", false, !isRoot)) {
                cmd!(NodeCommand.DeleteNode)(ctx);
            }
            if (icon) incTooltip(_(nodeActionToIcon!false("Delete")));

            if (igMenuItem(__(nodeActionToIcon!icon("Copy")), "", false, true)) {
                cmd!(NodeCommand.CopyNode)(ctx);
            }
            if (icon) incTooltip(_(nodeActionToIcon!false("Copy")));
        }
            
        if (igMenuItem(__(nodeActionToIcon!icon("Paste")), "", false, clipboardNodes.length > 0)) {
            cmd!(NodeCommand.PasteNode)(ctx);
        }
        if (icon) incTooltip(_(nodeActionToIcon!false("Paste")));

        if (igMenuItem(__(nodeActionToIcon!icon("Reload")), "", false, true)) {
            cmd!(NodeCommand.ReloadNode)(ctx);
        }
        if (icon) incTooltip(_(nodeActionToIcon!false("Reload")));

        static if (!isRoot) {
            if (igBeginMenu(__(nodeActionToIcon!icon("More Info")), true)) {
                if (selected.length > 1) {
                    foreach(sn; selected) {
                        
                        // %s is the name of the node in the More Info menu
                        // %u is the UUID of the node in the More Info menu
                        incText(_("%s ID: %u").format(sn.name, sn.uuid));

                        if (ExPart exp = cast(ExPart)sn) {
                            incText(_("%s Layer: %s").format(exp.name, exp.layerPath));
                        }
                    }
                } else {
                    // %u is the UUID of the node in the More Info menu
                    incText(_("ID: %u").format(n.uuid));

                    if (ExPart exp = cast(ExPart)n) {
                        incText(_("Layer: %s").format(exp.layerPath));
                    }
                }

                igEndMenu();
            }
            if (icon) incTooltip(_(nodeActionToIcon!false("More Info")));

            // Show which parameters use this node (by parameter IDs)
            if (igBeginMenu(__(nodeActionToIcon!icon("Parameter Usage")), true)) {
                bool anyFound = false;
                foreach (param; incActivePuppet().parameters) {
                    bool found = false;
                    foreach (binding; param.bindings) {
                        auto targetRes = binding.getTarget().target;
                        if (auto targetNode = cast(Node)targetRes) {
                            if (targetNode is n) { found = true; break; }
                        }
                    }
                    if (found) {
                        anyFound = true;
                        if (igMenuItem(param.name.toStringz, null, false, true)) {
                            Context pctx = new Context();
                            pctx.puppet = incActivePuppet();
                            pctx.parameters = [param];
                            cmd!(ParameditCommand.ToggleParameterArm)(pctx);
                        }
                    }
                }
                if (!anyFound) {
                    incText(_("No parameter uses this node"));
                }
                igEndMenu();
            }
            if (icon) incTooltip(_(nodeActionToIcon!false("Parameter Usage")));

            if (igMenuItem(__(nodeActionToIcon!icon("Recalculate origin")), "", false, true)) {
                cmd!(NodeCommand.CentralizeNode)(ctx);
            }
            if (icon) incTooltip(_(nodeActionToIcon!false("Recalculate origin")));

            auto fromType = ngGetCommonNodeType(incSelectedNodes);
            if (fromType in conversionMap) {
                if (igBeginMenu(__(nodeActionToIcon!icon("Convert To...")), true)) {
                    // Ensure bulk conversion uses the full current selection
                    if (selected.length > 0) ctx.nodes = selected;
                    foreach (toType; conversionMap[fromType]) {
                        incText(incTypeIdToIcon(toType));
                        igSameLine(0, 2);
                        if (igMenuItem(__(toType), "", false, true)) {
                            auto cmd = ensureConvertToCommand(toType);
                            cmd.run(ctx);
                        }
                    }
                    igEndMenu();
                }
                if (icon) incTooltip(_(nodeActionToIcon!false("Convert To ...")));
            }
        }
        if (title != null)
            igEndPopup();
    }
}

enum SelectState {
    Init, Started, Ended
}

struct SelectStateData {
    SelectState state;
    Node lastClick;
    Node shiftSelect;

    // tracking for closed nodes
    bool hasRenderedLastClick;
    bool hasRenderedShiftSelect;
}

/**
    The Nodes Tree Panel
*/
class NodesPanel : Panel {
private:
    string filter;
    bool[uint] filterResult;

    SelectStateData nextSelectState;
    SelectStateData curSelectState;
    Node[] rangeSelectNodes;
    bool selectStateUpdate = false;
    bool revserseOrder = false;
    bool pendingFocus = false;
    uint lastSelUuid = 0;

protected:
    /**
        track the last click and shift select if they are rendered
    */
    void trackingRenderedNode(ref Node node) {
        if (nextSelectState.lastClick is node)
            nextSelectState.hasRenderedLastClick = true;

        if (nextSelectState.shiftSelect is node)
            nextSelectState.hasRenderedShiftSelect = true;
    }

    void startTrackingRenderedNodes() {
        nextSelectState.hasRenderedLastClick = false;
        nextSelectState.hasRenderedShiftSelect = false;
    }

    void endTrackingRenderedNodes() {
        if (!nextSelectState.hasRenderedLastClick && nextSelectState.lastClick !is null) {
            nextSelectState.lastClick = null;
            nextSelectState.shiftSelect = null;
            selectStateUpdate = true;
        }

        if (!nextSelectState.hasRenderedShiftSelect && nextSelectState.shiftSelect !is null) {
            nextSelectState.shiftSelect = null;
            selectStateUpdate = true;
        }
    }

    void treeSetEnabled(Node n, bool enabled) {
        n.setEnabled(enabled);
        foreach(child; n.children) {
            treeSetEnabled(child, enabled);
        }
    }


    void toggleSelect(ref Node n) {
        if (incNodeInSelection(n))
            incRemoveSelectNode(n);
        else
            incAddSelectNode(n);

        rangeSelectNodes = [];
    }

    /**
        Select a range of nodes, it should be called when the user is holding shift key and click on a node
    */
    void selectRange(ref Node n) {
        if (curSelectState.lastClick is null) {
            nextSelectState.lastClick = n;
            incSelectNode(n);
            return;
        }

        // recover rangeSelectNodes if selected
        foreach(node; rangeSelectNodes) {
            incRemoveSelectNode(node);
        }
        rangeSelectNodes = [];

        nextSelectState.shiftSelect = n;
    }

    /**
        Handle range selection, this function should be called in the treeAddNode or recursive function
        we assume caller would traverse the tree nodes in order
    */
    void handleRangeSelect(ref Node n) {
        if (curSelectState.state == SelectState.Ended ||
            curSelectState.lastClick is null ||
            curSelectState.shiftSelect is null
            ) {
            return;
        }

        if (n == curSelectState.lastClick || n == curSelectState.shiftSelect) {
            switch(curSelectState.state) {
                case SelectState.Init:
                    curSelectState.state = SelectState.Started;
                    break;
                case SelectState.Started:
                    curSelectState.state = SelectState.Ended;
                    nextSelectState.shiftSelect = null;
                    break;
                default:
                    break;
            }
        }

        if (curSelectState.state != SelectState.Init && !incNodeInSelection(n)) {
            incAddSelectNode(n);
        }

        if (curSelectState.state != SelectState.Init && n != curSelectState.lastClick) {
            rangeSelectNodes ~= n;
        }
    }

    void treeAddNode(bool isRoot = false)(ref Node n) {
        if (!filterResult[n.uuid])
            return;

        Context ctx = new Context();
        ctx.puppet = incActivePuppet();
        ctx.nodes = [n];
        igTableNextRow();

        auto io = igGetIO();

        // // Draw Enabler for this node first
        // igTableSetColumnIndex(1);
        // igPushFont(incIconFont());
        //     incText(n.enabled ? "\ue8f4" : "\ue8f5");
        // igPopFont();


        // Prepare node flags
        ImGuiTreeNodeFlags flags;
        if (n.children.length == 0) flags |= ImGuiTreeNodeFlags.Leaf;
        flags |= ImGuiTreeNodeFlags.DefaultOpen;
        flags |= ImGuiTreeNodeFlags.OpenOnArrow;


        // Then draw the node tree index
        igTableSetColumnIndex(0);
        igSetNextItemWidth(8);
        bool open = igTreeNodeEx(cast(void*)n.uuid, flags, "");

            // Show node entry stuff
            igSameLine(0, 4);

            auto selectedNodes = incSelectedNodes();
            igPushID(n.uuid);
                    bool selected = incNodeInSelection(n);

                    igBeginGroup();
                        igIndent(4);

                        // Type Icon
                        static if (!isRoot) {
                            if (n.getEnabled()) incText(incTypeIdToIcon(n.typeId));
                            else incTextDisabled(incTypeIdToIcon(n.typeId));
                            if (igIsItemClicked()) {
                                cmd!(NodeCommand.ToggleVisibility)(ctx);
                            }
                        } else {
                            incText("");
                        }
                        igSameLine(0, 2);

                        handleRangeSelect(n);

                        // Selectable
                        if (igSelectable(isRoot ? __("Puppet") : n.name.toStringz, selected, ImGuiSelectableFlags.None, ImVec2(0, 0))) {
                            switch(incEditMode) {
                                default:
                                    selectStateUpdate = true;
                                    if (!io.KeyShift)
                                        nextSelectState.lastClick = n;

                                    if (io.KeyCtrl && !io.KeyShift)
                                        toggleSelect(n);
                                    else if (!io.KeyCtrl && io.KeyShift)
                                        selectRange(n);
                                    else if (selected && selectedNodes.length == 1)
                                        incFocusCamera(n);
                                    else
                                        incSelectNode(n);
                                    break;
                            }
                        }

                        trackingRenderedNode(n);
                        // Auto-focus when selection changes and feature is enabled
                        if (followSelection && pendingFocus && selected) {
                            igSetScrollHereY(0.25f);
                            pendingFocus = false;
                        }

                        if (igIsItemClicked(ImGuiMouseButton.Right)) {
                            igOpenPopup("NodeActionsPopup");
                        }

                        incNodeActionsPopup!("NodeActionsPopup", isRoot)(n);
                    igEndGroup();

                    static if (!isRoot) {
                        if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                            igSetDragDropPayload("_PUPPETNTREE", cast(void*)&n, (&n).sizeof, ImGuiCond.Always);
                            if (selectedNodes.length > 1) {
                                incDragdropNodeList(selectedNodes);
                            } else {
                                incDragdropNodeList(n);
                            }
                            igEndDragDropSource();
                        }
                    }
            igPopID();

            if(igBeginDragDropTarget()) {
                const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
                if (payload !is null) {
                    Node payloadNode = *cast(Node*)payload.Data;

                    Context pCtx = new Context();
                    pCtx.puppet = incActivePuppet();
                    pCtx.nodes = [payloadNode];
                    cmd!(NodeCommand.MoveNode)(pCtx, n, 0);
                    
                    if (open) igTreePop();
                    igEndDragDropTarget();
                    return;
                }
                igEndDragDropTarget();
            }

        if (open) {
            // Draw children
            void drawChildren(ref Node child, ulong i) {
                if (child.uuid !in filterResult)
                    return;

                if (!filterResult[child.uuid])
                    return;

                igPushID(cast(int)i);
                    igTableNextRow();
                    igTableSetColumnIndex(0);
                    igInvisibleButton("###TARGET", ImVec2(128, 4));

                    if(igBeginDragDropTarget()) {
                        const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
                        if (payload !is null) {
                            Node payloadNode = *cast(Node*)payload.Data;
                            
                            Context pCtx = new Context();
                            pCtx.puppet = incActivePuppet();
                            pCtx.nodes = [payloadNode];
                            cmd!(NodeCommand.MoveNode)(pCtx, n, i);
                                                        
                            igEndDragDropTarget();
                            igPopID();
                            igTreePop();
                            return;
                        }
                        igEndDragDropTarget();
                    }
                igPopID();

                treeAddNode(child);
            }

            if (revserseOrder) {
                foreach_reverse(i, child; n.children)
                    drawChildren(child, i);
            } else {
                foreach(i, child; n.children)
                    drawChildren(child, i);
            }
            igTreePop();
        }
        

    }

    bool filterNodes(Node n) {
        import std.algorithm;
        bool result = false;
        if (n.name.toLower.canFind(filter)) {
            result = true;
        } else if (n.children.length == 0) {
            result = false;
        }

        foreach(child; n.children) {
            result |= filterNodes(child);
        }

        filterResult[n.uuid] = result;
        return result;
    }

    override
    void onUpdate() {

        if (incEditMode == EditMode.ModelEdit) {
            if (!incArmedParameter && (igIsWindowFocused(ImGuiFocusedFlags.ChildWindows) || igIsWindowHovered(ImGuiHoveredFlags.ChildWindows))) {
                if (incShortcut("Ctrl+A")) {
                    incSelectAll();
                }
            }
        }

        if (incEditMode == EditMode.VertexEdit) {
            incLabelOver(_("In vertex edit mode..."), ImVec2(0, 0), true);
            return;
        }

        if (igBeginChild("NodesMain", ImVec2(0, -30), false)) {
            // Detect selection change to schedule focus
            auto _sel = incSelectedNodes();
            uint curSel = _sel.length > 0 ? _sel[0].uuid : 0;
            if (curSel != lastSelUuid) {
                lastSelUuid = curSel;
                pendingFocus = true;
            }
            
            // temp variables
            float scrollDelta = 0;
            auto avail = incAvailableSpace();

            // Get the screen position of our node window
            // as well as the size for the drag/drop scroll
            ImVec2 screenPos;
            igGetCursorScreenPos(&screenPos);
            ImRect crect = ImRect(
                screenPos,
                ImVec2(screenPos.x+avail.x, screenPos.y+avail.y)
            );

            // Handle figuring out whether the user is trying to scroll the list via drag & drop
            // We're only peeking in to the contents of the payload.
            incBeginDragDropFake();
                auto data = igAcceptDragDropPayload("_PUPPETNTREE", ImGuiDragDropFlags.AcceptPeekOnly | ImGuiDragDropFlags.SourceAllowNullID);
                if (igIsMouseDragging(ImGuiMouseButton.Left) && data && data.Data) {
                    ImVec2 mousePos;
                    igGetMousePos(&mousePos);

                    // If mouse is inside the window
                    if (mousePos.x > crect.Min.x && mousePos.x < crect.Max.x) {
                        float scrollSpeed = (4*60)*deltaTime();

                        if (mousePos.y < crect.Min.y+32 && mousePos.y >= crect.Min.y) scrollDelta = -scrollSpeed;
                        if (mousePos.y > crect.Max.y-32 && mousePos.y <= crect.Max.y) scrollDelta = scrollSpeed;
                    }
                }
            incEndDragDropFake();

            igPushStyleVar(ImGuiStyleVar.CellPadding, ImVec2(4, 1));
            igPushStyleVar(ImGuiStyleVar.IndentSpacing, 14);

            if (incInputText("Node Filter", filter)) {
                filter = filter.toLower;
                filterResult.clear();
            }

            incTooltip(_("Filter, search for specific nodes"));

            // filter nodes
            filterNodes(incActivePuppet.root);

            if (igBeginTable("NodesContent", 2, ImGuiTableFlags.ScrollX, ImVec2(0, 0), 0)) {
                auto window = igGetCurrentWindow();
                igSetScrollY(window.Scroll.y+scrollDelta);
                igTableSetupColumn("Nodes", ImGuiTableColumnFlags.WidthFixed, 0, 0);
                //igTableSetupColumn("Visibility", ImGuiTableColumnFlags_WidthFixed, 32, 1);
                
                if (incEditMode == EditMode.ModelEdit) {
                    if (selectStateUpdate) {
                        curSelectState = nextSelectState;
                        curSelectState.state = SelectState.Init;
                        selectStateUpdate = false;
                    }

                    startTrackingRenderedNodes();
                    igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(4, 4));
                        treeAddNode!true(incActivePuppet.root);
                    igPopStyleVar();
                    endTrackingRenderedNodes();
                }

                igEndTable();
            }
            if (igIsItemClicked(ImGuiMouseButton.Left)) {
                incSelectNode(null);
            }
            igPopStyleVar();
            igPopStyleVar();
        }
        igEndChild();

        igSeparator();
        igSpacing();
        
        if (incEditMode() == EditMode.ModelEdit) {
            auto selected = incSelectedNodes();
            if (incButtonColored("", ImVec2(24, 24))) {
                foreach(payloadNode; selected) incDeleteChildWithHistory(payloadNode);
                // should clean up selection, prevents unexpected behaviour
                incSelectNode(null);
            }
            incTooltip(_("Delete selected nodes"));

            igSameLine(0, 2);
            if (incButtonColored("\ue164##SortNodeOrder", ImVec2(24, 24), revserseOrder ? ImVec4.init : ImVec4(0.6f, 0.6f, 0.6f, 1f))) {
                revserseOrder = !revserseOrder;
            }
            incTooltip(_("Reverse Node Order"));

            igSameLine(0, 2);
            if (incButtonColored("\ue87a###FollowSelection", ImVec2(24, 24), followSelection ? ImVec4.init : ImVec4(0.6f, 0.6f, 0.6f, 1f))) {
                followSelection = !followSelection;
            }
            incTooltip(_("Follow Selection"));

            if(igBeginDragDropTarget()) {
                const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
                if (payload !is null) {
                    Node payloadNode = *cast(Node*)payload.Data;

                    if (selected.length > 1) {
                        foreach(pn; selected) incDeleteChildrenWithHistory(selected);
                        incSelectNode(null);
                    } else {

                        // Make sure we don't keep selecting a node we've removed
                        if (incNodeInSelection(payloadNode)) {
                            incSelectNode(null);
                        }

                        incDeleteChildWithHistory(payloadNode);
                    }
                    
                    igPopFont();
                    return;
                }
                igEndDragDropTarget();
            }
        }

    }

public:

    this() {
        super("Nodes", _("Nodes"), true);
        flags |= ImGuiWindowFlags.NoScrollbar;
        activeModes = EditMode.ModelEdit;
    }
}

/**
    Generate nodes frame
*/
mixin incPanel!NodesPanel;
