module nijigenerate.commands.node.dynamic;

import nijigenerate.commands.base;
import nijigenerate.commands.node.node : AddNodeCommand, InsertNodeCommand;
import nijigenerate.commands.node.base : conversionMap, ngGetCommonNodeType, ngConvertTo; // known types + helpers
import nijilive; // inInstantiateNode
import i18n;

// Stable key type for Node-type-based commands (avoids generic string)
struct NodeTypeKey {
    string name;     // class name
    string suffix;   // optional suffix
    string toString() const { return suffix.length ? (name ~ "|" ~ suffix) : name; }
    size_t toHash() const @safe nothrow @nogc {
        import core.internal.hash : hashOf;
        return hashOf(name) ^ (hashOf(suffix) * 1315423911u);
    }
    bool opEquals(const NodeTypeKey rhs) const @safe nothrow @nogc {
        return name == rhs.name && suffix == rhs.suffix;
    }
}

AddNodeCommand[NodeTypeKey] addNodeCommands;
InsertNodeCommand[NodeTypeKey] insertNodeCommands;

// Convert-To dynamic commands per destination type; source type is derived from context
struct ConvertToKey {
    string toType;
    string toString() const { return toType; }
    size_t toHash() const @safe nothrow @nogc { import core.internal.hash : hashOf; return hashOf(toType); }
    bool opEquals(const ConvertToKey rhs) const @safe nothrow @nogc { return toType == rhs.toType; }
}

class ConvertNodeToCommand : ExCommand!(
    TW!(string, "toType", "destination node type")
) {
    this(string toType) {
        super("Convert To " ~ toType, "Convert To " ~ toType, toType);
        import std.stdio;
        writefln("New class Convert to : %s", this.toType);
    }
    override bool runnable(Context ctx) {
        if (!ctx.hasNodes || ctx.nodes.length == 0) return false;
        auto from = ngGetCommonNodeType(ctx.nodes);
        if (!from) return false;
        if (auto p = from in conversionMap) {
            foreach (v; *p) if (v == toType) return true;
        }
        return false;
    }
    override CreateResult!Node run(Context ctx) {
        if (!runnable(ctx)) return new CreateResult!Node(false, null, "Context not convertible");
        auto before = ctx.nodes.dup;
        auto converted = ngConvertTo(ctx.nodes, toType);
        return new CreateResult!Node(converted.length > 0, converted, converted.length ? ("Nodes converted from "~before.length.stringof) : "No nodes converted");
    }
}

ConvertNodeToCommand[ConvertToKey] convertNodeCommands;

private bool tryInstantiateNode(string className)
{
    // Try to instantiate once to validate availability
    Node n;
    try {
        static if (__traits(compiles, { n = inInstantiateNode(className, null); })) {
            n = inInstantiateNode(className, null);
        } else static if (__traits(compiles, { n = inInstantiateNode(className); })) {
            n = inInstantiateNode(className);
        } else {
            return false;
        }
    } catch (Throwable) {
        return false;
    }
    return n !is null;
}

private string[] discoverNodeTypes()
{
    // Manually list node types as defined in nodes panel menu (ngAddOrInsertNodeMenu)
    // Keep this in sync with UI
    return [
        "Node",
        "Mask",
        "Composite",
        "SimplePhysics",
        "MeshGroup",
        "DynamicComposite",
        "PathDeformer",
        "GridDeformer",
        "Camera",
    ];
}

AddNodeCommand ensureAddNodeCommand(string className, string suffix = null)
{
    NodeTypeKey key = NodeTypeKey(className, suffix);
    if (auto p = key in addNodeCommands) return *p;
    auto cmd = new AddNodeCommand(className, suffix);
    addNodeCommands[key] = cmd;
    return cmd;
}

InsertNodeCommand ensureInsertNodeCommand(string className, string suffix = null)
{
    NodeTypeKey key = NodeTypeKey(className, suffix);
    if (auto p = key in insertNodeCommands) return *p;
    auto cmd = new InsertNodeCommand(className, suffix);
    insertNodeCommands[key] = cmd;
    return cmd;
}

// Pre-populate on startup like mesheditor/tool
void ngInitCommands(T)() if (is(T == NodeTypeKey))
{
    foreach (t; discoverNodeTypes()) {
        ensureAddNodeCommand(t);
        ensureInsertNodeCommand(t);
    }
}

// Register all ConvertTo commands for each destination type
void ngInitCommands(T)() if (is(T == ConvertToKey))
{
    string[string] added;
    foreach (from, arr; conversionMap) {
        foreach (to; arr) {
            if (to in added) continue;
            added[to] = to;
            ConvertToKey key = ConvertToKey(to);
            if (auto p = key in convertNodeCommands) continue;
            auto cmd = cast(Command) new ConvertNodeToCommand(to);
            convertNodeCommands[key] = cmd;
        }
    }
}

ConvertNodeToCommand ensureConvertToCommand(string toType)
{
    ConvertToKey key = ConvertToKey(toType);
    if (auto p = key in convertNodeCommands) {
        auto cnv = *p;
        if (cnv.toType != toType) {
            cnv.toType = toType;
        }
        return *p;
    }
    auto cmd = new ConvertNodeToCommand(toType);
    convertNodeCommands[key] = cmd;
    return cmd;
}
