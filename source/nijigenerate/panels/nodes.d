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

/**
    The logger frame
*/
class NodesPanel : Panel {
protected:
    void treeSetEnabled(Node n, bool enabled) {
        n.setEnabled(enabled);
        foreach(child; n.children) {
            treeSetEnabled(child, enabled);
        }
    }


    void treeAddNode(bool isRoot = false)(ref Node n) {
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

                        // Selectable
                        if (igSelectable(isRoot ? __("Puppet") : n.name.toStringz, selected, ImGuiSelectableFlags.None, ImVec2(0, 0))) {
                            switch(incEditMode) {
                                default:
                                    if (selected) {
                                        if (incSelectedNodes().length > 1) {
                                            if (io.KeyCtrl) incRemoveSelectNode(n);
                                            else incSelectNode(n);
                                        } else {
                                            incFocusCamera(n);
                                        }
                                    } else {
                                        if (io.KeyCtrl) incAddSelectNode(n);
                                        else incSelectNode(n);
                                    }
                                    break;
                            }
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
            foreach(i, child; n.children) {
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
            igTreePop();
        }
        

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

            if (igBeginTable("NodesContent", 2, ImGuiTableFlags.ScrollX, ImVec2(0, 0), 0)) {
                auto window = igGetCurrentWindow();
                igSetScrollY(window.Scroll.y+scrollDelta);
                igTableSetupColumn("Nodes", ImGuiTableColumnFlags.WidthFixed, 0, 0);
                //igTableSetupColumn("Visibility", ImGuiTableColumnFlags_WidthFixed, 32, 1);
                
                if (incEditMode == EditMode.ModelEdit) {
                    igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(4, 4));
                        treeAddNode!true(incActivePuppet.root);
                    igPopStyleVar();
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
            }

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
