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
import core.exception;


//==================================================================================
// Command Palette Definition for Node
//==================================================================================

class AddNodeCommand : ExCommand!(
        TW!(string, "className", "class name of new node."), 
        TW!(string, "_suffix", "suffix pattern for new node")) {
    this(string className, string _suffix = null) {
        // Dynamic label; keep as-is for now (static part is translatable elsewhere)
        super(null, "Add Node " ~ className, className, _suffix);
    }

    override
    CreateResult!Node run(Context ctx) {
        Node[] created = null;
        try {
            if (ctx.hasNodes) {
                created = ngAddNodes(ctx.nodes, className, _suffix);
            } else if (ctx.hasPuppet && ctx.puppet !is null) {
                created = ngAddNodes([ctx.puppet.root], className, _suffix);
            }
        } catch (RangeError e) {
            return new CreateResult!Node(false, null, "Failed to add node");
        }
        return new CreateResult!Node(created.length > 0, created, created.length ? null : "No nodes created");
    }
}

class InsertNodeCommand : ExCommand!(
        TW!(string, "className", "class name of new node."), 
        TW!(string, "_suffix", "suffix pattern for new node")) {
    this(string className, string _suffix = null) {
        super(null, "Insert Node " ~ className, className, _suffix);
    }

    override
    CreateResult!Node run(Context ctx) {
        Node[] created = null;
        try {
            if (ctx.hasNodes) {
                created = ngInsertNodes(ctx.nodes, className, _suffix);
            } else if (ctx.hasPuppet && ctx.puppet !is null) {
                created = ngInsertNodes([ctx.puppet.root], className, _suffix);
            }
        } catch (RangeError e) {
            return new CreateResult!Node(false, null, "Failed to insert node");
        }
        return new CreateResult!Node(created.length > 0, created, created.length ? null : "No nodes inserted");
    }
}

class MoveNodeCommand : ExCommand!(
        TW!(Node, "newParent", "new parent node"), 
        TW!(ulong, "index", "index in new parent node")) {
    this(Node newParent, ulong index) {
        super(null, _("Move Node "), newParent, index);
    }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0) return CommandResult(false, "No nodes");
        auto selectedNodes = incSelectedNodes();
        auto child = ctx.nodes[0];
        try {
            if (incNodeInSelection(child)) incMoveChildrenWithHistory(selectedNodes, newParent, 0);
            else incMoveChildWithHistory(child, newParent, 0);
        } catch (Exception ex) {
            incDialog(__("Error"), ex.msg);
            return CommandResult(false, ex.msg);
        }
        return CommandResult(true);
    }
}

class ConvertToCommand : ExCommand!(TW!(string, "className", "new class name for node")) {
    this(string className) {
        super(null, "Convert Node to "~className, className);
    }

    override
    CreateResult!Node run(Context ctx) {
        if (!ctx.hasNodes) return new CreateResult!Node(false, null, "No nodes");

        auto before = ctx.nodes.dup;
        auto converted = ngConvertTo(ctx.nodes, className);
        return new CreateResult!Node(converted.length > 0, converted, converted.length ? ("Nodes converted from " ~ before.length.stringof) : "No nodes converted");
    }
}

class DeleteNodeCommand : ExCommand!() {
    this() { super(null, _("Delete Node")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return CommandResult(false, "No nodes");

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
        return CommandResult(true);
    }
}

class CutNodeCommand : ExCommand!() {
    this() { super(_("Cut"), _("Cut Node")); }

    override 
    CommandResult run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return CommandResult(false, "No nodes");
        /*
        auto n = ctx.nodes[0];
        auto selected = incSelectedNodes();
        if (selected.length > 0)
            copyToClipboard(selected);
        else
            copyToClipboard([n]);
        */
        return CommandResult(true);
    }

    override bool runnable(Context ctx) {
        // Cut not yet implemented; keep disabled via runnable
        return false;
    }
}

class CopyNodeCommand : ExCommand!() {
    this() { super(_("Copy"), _("Copy Node")); }

    override 
    CommandResult run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return CommandResult(false, "No nodes");

        auto n = ctx.nodes[0];
        auto selected = incSelectedNodes();
        if (selected.length > 0)
            copyToClipboard(selected);
        else
            copyToClipboard([n]);
        return CommandResult(true);
    }

    override bool runnable(Context ctx) {
        return ctx.hasNodes && ctx.nodes.length > 0;
    }
}

class PasteNodeCommand : ExCommand!() {
    this() { super(_("Paste"), _("Paste Node")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return CommandResult(false, "No nodes");

        auto n = ctx.nodes[0];
        if (clipboardNodes.length > 0) {
            pasteFromClipboard(n);
            return CommandResult(true);
        }
        return CommandResult(false, "Clipboard empty");
    }

    override bool runnable(Context ctx) {
        return ctx.hasNodes && ctx.nodes.length > 0 && clipboardNodes.length > 0;
    }
}

class ReloadNodeCommand : ExCommand!() {
    this() { super(_("Reload Node")); }

    override
    CommandResult run(Context ctx) {
        if (ctx.hasNodes) {
            incReloadNode(ctx.nodes);
            return CommandResult(true);
        }
        return CommandResult(false, "No nodes");
    }
}

class VertexModeCommand : ExCommand!() {
    this() { super(_("Edit Vertex")); }

    override
    CommandResult run(Context ctx) {
        if (ctx.hasNodes && ctx.nodes.length > 0) {
            Node n = ctx.nodes[0];
            if (auto d = cast(Deformable)n) {
                if (!incArmedParameter()) {
                    incVertexEditStartEditing(d);
                }
            }
        }
        return CommandResult(true);
    }
}

class ToggleVisibilityCommand : ExCommand!() {
    this() { super(_("Toggle Visibility")); }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return CommandResult(false, "No nodes");
        auto n = ctx.nodes[0];
        n.setEnabled(!n.getEnabled());
        return CommandResult(true);
    }
}

class CentralizeNodeCommand : ExCommand!() {
    this() { super(_("Centralize Node")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0)
            return CommandResult(false, "No nodes");

        auto n = ctx.nodes[0];
        n.centralize();
        return CommandResult(true);
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
