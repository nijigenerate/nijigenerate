module nijigenerate.commands.node.node;

import nijigenerate.commands.base;

import nijigenerate.viewport.vertex;
import nijigenerate.widgets.dragdrop;
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

private {
    Node[] clipboardNodes;

    void copyToClipboard(Node[] nodes) {
        clipboardNodes.length = 0;
        foreach (node; nodes) {
            auto newNode = node.dup;
            clipboardNodes ~= newNode;
        }
    }

    void pasteFromClipboard(Node parent) {
        if (parent !is null) {
            incActionPush(new NodeMoveAction(clipboardNodes, parent, 0));
            foreach (node; clipboardNodes) {
                incReloadNode([node]);
            }
            clipboardNodes.length = 0;
        }
    }

    string[][string] conversionMap;

    void insertNodesAux(Node[] parents, Node[] children, string className, string suffixName) {
        if (children && parents.length != children.length)
            children = null;
        incActionPushGroup();
        foreach (i, p; parents) {
            Node newChild = inInstantiateNode(className, null);
            string nodeName = null;
            if (suffixName && suffixName.length != 0) {
                if (children)
                    nodeName = children[i].name ~ suffixName;
                else
                    nodeName = p.name ~ suffixName;
            }
            incAddChildWithHistory(newChild, p, nodeName);
            if (children) {
                newChild.localTransform.translation = children[i].localTransform.translation;
                newChild.transformChanged();
                incActionPush(new NodeMoveAction([children[i]], newChild));
            }
        }
        incActionPopGroup();
    }
}

void incReloadNode(Node[] nodes) {
    foreach (node; nodes) {
        incReloadNode(node.children);
        foreach (child; node.children) {
            child.notifyChange(child);
        }
        node.clearCache();
    }
}

void ngAddNodes(Node[] parents, string className, string _suffixName = null) {
    insertNodesAux(parents, null, className, _suffixName);
}

void ngInsertNodes(Node[] children, string className, string _suffixName = null) {
    insertNodesAux(children.map!((v)=>v.parent).array, children, className, _suffixName);
}

string ngGetCommonNodeType(Node[] nodes) {
    string type = null;
    foreach (node; nodes) {
        if (!type) { 
            type = node.typeId; 
        } else if (type != node.typeId) {
            return null;
        }
    }
    return type;
}

void ngConvertTo(Node[] nodes, string toType) {
    if (!toType) return;
    if (nodes.length == 0) return;

    auto group = new GroupAction();
    Node[] newNodes = [];
    foreach (node; nodes.dup) {
        Node newNode = inInstantiateNode(toType);
        newNode.copyFrom(node, true, false);
        group.addAction(new NodeReplaceAction(node, newNode, true));
        newNodes ~= newNode;
        newNode.notifyChange(newNode, NotifyReason.StructureChanged);
    }
    incActionPush(group);
    incSelectNodes(newNodes);
}

//==================================================================================
// Command Palette Definition for Node
//==================================================================================

class AddNodeCommand : ExCommand!(string, string) {
    this(string className, string _suffix = null) {
        super("Add Node " ~ className, className, _suffix);
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes)
            ngAddNodes(ctx.nodes, arg0, arg1);
    }
}

class InsertNodeCommand : ExCommand!(string, string) {
    this(string className, string _suffix = null) {
        super("Insert Node " ~ className, className, _suffix);
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes)
            ngInsertNodes(ctx.nodes, arg0, arg1);
    }
}

class ConvertToCommand : ExCommand!(string) {
    this(string className) {
        super("Convert Node to "~className, className);
    }

    override
    void run(Context ctx) {
        if (ctx.hasNodes)
            ngConvertTo(ctx.nodes, arg0);
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
    Add,
    Insert,
    Delete,
    ConvertTo,
    Copy,
    Paste,
    Reload,
    VertexMode,
    ToggleVisibility,
    Centralize
}


private {
    Command[NodeCommand] commands;

    static this() {
        conversionMap = [
            "Node": ["MeshGroup", "DynamicComposite"],
            "DynamicComposite": ["MeshGroup", "Node", "Part", "Composite"],
            "MeshGroup": ["DynamicComposite", "Node"],
            "Composite": ["DynamicComposite", "Node"]
        ];

        commands[NodeCommand.Add] = new AddNodeCommand("", "");
        commands[NodeCommand.Insert] = new InsertNodeCommand("", "");
        commands[NodeCommand.Delete] = new DeleteNodeCommand();
        commands[NodeCommand.ConvertTo] = new ConvertToCommand("");
        commands[NodeCommand.Copy] = new CopyNodeCommand();
        commands[NodeCommand.Paste] = new PasteNodeCommand();
        commands[NodeCommand.Reload] = new ReloadNodeCommand();
        commands[NodeCommand.VertexMode] = new VertexModeCommand();
        commands[NodeCommand.ToggleVisibility] = new ToggleVisibilityCommand();
        commands[NodeCommand.Centralize] = new CentralizeNodeCommand();
    }
//NodeCreateMenu("Node", _("Node"));
//NodeCreateMenu("Mask", _("Mask"));
//NodeCreateMenu("Composite", _("Composite"));
//NodeCreateMenu("SimplePhysics", _("Simple Physics"));
//NodeCreateMenu("MeshGroup", _("Mesh Group"));
//NodeCreateMenu("DynamicComposite", _("Dynamic Composite"));
//NodeCreateMenu("PathDeformer", _("Path Deformer"));
//NodeCreateMenu("Camera", _("Camera"));

}