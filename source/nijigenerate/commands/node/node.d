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


// コマンド登録用 mixin 定義（文字列引数方式）
template register(alias id, string args = "") {
    import std.string : format;
    // Extract the enum member name (e.g., "Add" from NodeCommand.Add)
    enum name = __traits(identifier, id);
    enum ctor = name ~ "Command";
    static if (args == "")
        enum register = format("commands[NodeCommand.%s] = new %s();", name, ctor);
    else
        enum register = format("commands[NodeCommand.%s] = new %s(%s);", name, ctor, args);
}

// 引数なしで new できるかをチェック
template canDefaultConstruct(T) {
    enum canDefaultConstruct = __traits(compiles, new T());
}

// NodeCommand から FooCommand 型を生成
template GetCommandType(alias enumValue) {
    mixin("alias GetCommandType = " ~ __traits(identifier, enumValue) ~ "Command;");
}

private {
    Command[NodeCommand] commands;

    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!NodeCommand) {
            static if (canDefaultConstruct!(GetCommandType!name))
            {
                mixin(register!(name));
            }
        }

        // 引数ありのコンストラクタを持つコマンドは手動登録
        mixin(register!(NodeCommand.AddNode, `"null"` ~ ", " ~ `"null"`));
        mixin(register!(NodeCommand.InsertNode, `"null"` ~ ", " ~ `"null"`));
        mixin(register!(NodeCommand.ConvertTo, `"null"`));

        import std.stdio;
        writefln("\nnode");
        foreach (k, v; commands) {
            writefln("%s: %s", k, v);
        }
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
