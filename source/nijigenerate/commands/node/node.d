module nijigenerate.commands.node.node;

import nijigenerate.commands.base;
import nijigenerate.commands.node.base;

import nijigenerate.ext;
import nijigenerate;
import nijigenerate.core;
import nijigenerate.actions;
import nijigenerate.viewport.vertex;
import nijilive;


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

class CopyNodeCommand : ExCommand!() {
    this() { super("Copy Node"); }

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
    this() { super("Paste Node"); }
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
    ConvertTo,
    CopyNode,
    PasteNode,
    ReloadNode,
    VertexMode,
    ToggleVisibility,
    CentralizeNode
}


Command[NodeCommand] commands;
private {

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!NodeCommand) {
            static if (__traits(compiles, { mixin(registerCommand!(name)); }))
                mixin(registerCommand!(name));
        }

        mixin(registerCommand!(NodeCommand.AddNode, null, null));
        mixin(registerCommand!(NodeCommand.InsertNode, null, null));
        mixin(registerCommand!(NodeCommand.ConvertTo, null));
    }
}
