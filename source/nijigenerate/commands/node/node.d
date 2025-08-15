module nijigenerate.commands.node.node;

import nijigenerate.commands.base;
import nijigenerate.commands.node.base;

import nijigenerate.ext;
import nijigenerate;
import nijigenerate.core;
import nijigenerate.actions;
import nijigenerate.viewport.vertex;
import nijilive;
import nijigenerate.widgets;
import i18n;


//==================================================================================
// Command Palette Definition for Node
//==================================================================================

class AddNodeCommand : ExCommand!(
        TW!(string, "className", "class name of new node."), 
        TW!(string, "_suffix", "suffix pattern for new node")) {
    this(string className, string _suffix = null) {
        super("Add Node " ~ className, className, _suffix);
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes)
            ngAddNodes(ctx.nodes, className, _suffix);
    }
}

class InsertNodeCommand : ExCommand!(
        TW!(string, "className", "class name of new node."), 
        TW!(string, "_suffix", "suffix pattern for new node")) {
    this(string className, string _suffix = null) {
        super("Insert Node " ~ className, className, _suffix);
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes)
            ngInsertNodes(ctx.nodes, className, _suffix);
    }
}

class MoveNodeCommand : ExCommand!(
        TW!(Node, "newParent", "new parent node"), 
        TW!(ulong, "index", "index in new parent node")) {
    this(Node newParent, ulong index) {
        super("Move Node ", newParent, index);
    }

    override
    void run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0) return;
        auto selectedNodes = incSelectedNodes();
        auto child = ctx.nodes[0];
        try {
            if (incNodeInSelection(child)) incMoveChildrenWithHistory(selectedNodes, newParent, 0);
            else incMoveChildWithHistory(child, newParent, 0);
        } catch (Exception ex) {
            incDialog(__("Error"), ex.msg);
        }

    }
}

class ConvertToCommand : ExCommand!(TW!(string, "className", "new class name for node")) {
    this(string className) {
        super("Convert Node to "~className, className);
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes)
            ngConvertTo(ctx.nodes, className);
    }
}

class DeleteNodeCommand : ExCommand!() {
    this() { super("Delete Node"); }
    override
    void run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return;

        auto n = ctx.nodes[0];
        auto selected = incSelectedNodes();
        if (selected.length > 1) {
            incDeleteChildrenWithHistory(selected);
            incSelectNode(null);
        } else {

            // Make sure we don't keep selecting a node we've removed
            if (incNodeInSelection(n)) {
                incSelectNode(null);
            }

            incDeleteChildWithHistory(n);
        }
        
        // Make sure we don't keep selecting a node we've removed
        incSelectNode(null);
    }
}

class CutNodeCommand : ExCommand!() {
    this() { super("Cut", "Cut Node"); }

    override 
    void run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return;
        /*
        auto n = ctx.nodes[0];
        auto selected = incSelectedNodes();
        if (selected.length > 0)
            copyToClipboard(selected);
        else
            copyToClipboard([n]);
        */
    }
}

class CopyNodeCommand : ExCommand!() {
    this() { super("Copy", "Copy Node"); }

    override 
    void run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return;

        auto n = ctx.nodes[0];
        auto selected = incSelectedNodes();
        if (selected.length > 0)
            copyToClipboard(selected);
        else
            copyToClipboard([n]);
    }
}

class PasteNodeCommand : ExCommand!() {
    this() { super("Paste", "Paste Node"); }
    override
    void run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return;

        auto n = ctx.nodes[0];
        if (clipboardNodes.length > 0) {
            pasteFromClipboard(n);
        }
    }
}

class ReloadNodeCommand : ExCommand!() {
    this() {
        super("Reload Node");
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes)
            incReloadNode(ctx.nodes);
    }
}

class VertexModeCommand : ExCommand!() {
    this() {
        super("Edit Vertex");
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes && ctx.nodes.length > 0) {
            Node n = ctx.nodes[0];
            if (auto d = cast(Deformable)n) {
                if (!incArmedParameter()) {
                    incVertexEditStartEditing(d);
                }
            }
        }
    }
}

class ToggleVisibilityCommand : ExCommand!() {
    this() {
        super("Toggle Visibility");
    }

    override
    void run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return;
        auto n = ctx.nodes[0];
        n.setEnabled(!n.getEnabled());
    }
}

class CentralizeNodeCommand : ExCommand!() {
    this() { super("Centralize Node"); }
    override
    void run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return;

        auto n = ctx.nodes[0];
        n.centralize();
    }
}

enum NodeCommand {
    AddNode,
    InsertNode,
    DeleteNode,
    MoveNode,
    ConvertTo,
    CutNode,
    CopyNode,
    PasteNode,
    ReloadNode,
    VertexMode,
    ToggleVisibility,
    CentralizeNode
}


Command[NodeCommand] commands;

void ngInitCommands(T)() if (is(T == NodeCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!NodeCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    mixin(registerCommand!(NodeCommand.AddNode, null, null));
    mixin(registerCommand!(NodeCommand.InsertNode, null, null));
    mixin(registerCommand!(NodeCommand.ConvertTo, null));
    mixin(registerCommand!(NodeCommand.MoveNode, null, 0));
}
