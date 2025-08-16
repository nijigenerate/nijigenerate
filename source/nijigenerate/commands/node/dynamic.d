module nijigenerate.commands.node.dynamic;

import nijigenerate.commands.base;
import nijigenerate.commands.node.node : AddNodeCommand, InsertNodeCommand;
import nijigenerate.commands.node.base : conversionMap; // known types
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

Command[NodeTypeKey] addNodeCommands;
Command[NodeTypeKey] insertNodeCommands;

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
    // Collect candidate types from conversionMap (keys + values)
    string[string] set;
    foreach (k, arr; conversionMap) {
        set[k] = k;
        foreach (v; arr) set[v] = v;
    }
    string[] types = set.values;
    // Filter by actual availability
    string[] ok;
    foreach (t; types) {
        if (tryInstantiateNode(t)) ok ~= t;
    }
    import std.algorithm.sorting : sort;
    ok.sort;
    return ok;
}

Command ensureAddNodeCommand(string className, string suffix = null)
{
    NodeTypeKey key = NodeTypeKey(className, suffix);
    if (auto p = key in addNodeCommands) return *p;
    auto cmd = cast(Command) new AddNodeCommand(className, suffix);
    addNodeCommands[key] = cmd;
    return cmd;
}

Command ensureInsertNodeCommand(string className, string suffix = null)
{
    NodeTypeKey key = NodeTypeKey(className, suffix);
    if (auto p = key in insertNodeCommands) return *p;
    auto cmd = cast(Command) new InsertNodeCommand(className, suffix);
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

