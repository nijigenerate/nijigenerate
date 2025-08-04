module nijigenerate.commands.base;

import nijilive;
import nijigenerate.core;
import nijigenerate.ext;

class Context {
    Puppet puppet;
    Node[] nodes;
    Parameter[] parameters;
    ParameterBinding[] bindings;
    vec2u keyPoint;
    enum ContextMask {
        None = 0,
        HasPuppet = 1,
        HasNodes = 2,
        HasParameters = 4,
        HasBindings = 8,
        HasKeyPoint = 16,
    };
    ContextMask masks;

    bool hasPuppet() { return (masks & ContextMask.HasPuppet) != 0; }
    bool hasNodes() { return (masks & ContextMask.HasNodes) != 0; }
    bool hasParameters() { return (masks & ContextMask.HasParameters) != 0; }
    bool hasBindings() { return (masks & ContextMask.HasBindings) != 0; }
    bool hasKeyPoint() { return (masks & ContextMask.HasKeyPoint) != 0; }

    void hasPuppet(bool value) { masks = (ContextMask)(~ContextMask.HasPuppet | (value? ContextMask.HasPuppet: ContextMask.None)); }
    void hasNodes(bool value) { masks = (ContextMask)(~ContextMask.HasNodes | (value? ContextMask.HasNodes: ContextMask.None)); }
    void hasParameters(bool value) { masks = (ContextMask)(~ContextMask.HasParameters | (value? ContextMask.HasParameters: ContextMask.None)); }
    void hasBindings(bool value) { masks = (ContextMask)(~ContextMask.HasBindings | (value? ContextMask.HasBindings: ContextMask.None)); }
    void hasKeyPoint(bool value) { masks = (ContextMask)(~ContextMask.HasKeyPoint | (value? ContextMask.HasKeyPoint: ContextMask.None)); }
}

interface Command {
    void run(Context context);
    string desc();
}

class ExCommand(T...) : Command {
    string _desc;
    mixin(generateMembers!T);

    static string generateMembers(U...)() {
        import std.conv : to;
        string code;
        foreach (i, Type; U) {
            code ~= Type.stringof ~" arg" ~ i.to!string ~ ";\n";
        }
        return code;
    }

    void assignMembers(U...)(U args) {
        import std.conv : to;
        static foreach (i; 0 .. U.length) {
            mixin("this.arg" ~ i.to!string ~ " = args[" ~ i.to!string ~ "];");
        }
    }

    this(string desc, T args) {
        _desc = desc;
        assignMembers!T(args);
    }
    
    string desc() { return _desc; }
    void run(Context context) {}
}
