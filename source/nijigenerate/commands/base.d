module nijigenerate.commands.base;

import std.traits : isInstanceOf, TemplateArgsOf, BaseClassesTuple, fullyQualifiedName;
import std.meta : staticMap, AliasSeq;
import std.array : join;
import std.format : format;
import std.conv : to;
import std.exception : enforce;

string toCodeString(T)(T arg) {
    import std.traits : isSomeString, isIntegral, isFloatingPoint;
    import std.conv : text;
    static if (isSomeString!T) {
        import std.string : replace;
        return `"` ~ (cast(string)arg).replace(`"`, `\"`) ~ `"`;
    } else static if (is(T == typeof(null))) {
        return "null";
    } else static if (is(T == enum)) {
        return T.stringof ~ "." ~ text(arg);
    } else {
        return text(arg);
    }
}

template registerCommand(alias id, Args...) {
    template ArgsToString(Args...) {
        static if (Args.length == 0)
            enum ArgsToString = "";
        else {
            import std.meta : AliasSeq;
            enum string ArgsToString = argsToStringImplHelper!(AliasSeq!Args);
        }
    }
    template argsToStringImplHelper(Args...) {
        static if (Args.length == 0)
            enum string argsToStringImplHelper = "";
        else static if (Args.length == 1)
            enum string argsToStringImplHelper = toCodeString(Args[0]);
        else
            enum string argsToStringImplHelper = toCodeString(Args[0]) ~ ", " ~ argsToStringImplHelper!(Args[1 .. $]);
    }
    enum string EnumType = typeof(id).stringof;
    static if (Args.length == 0) {
        enum registerCommand = format(`commands[%s.%s] = new %sCommand();`, EnumType, id.stringof, id.stringof);
    } else {
        enum registerCommand = format(
            `commands[%s.%s] = new %sCommand(%s);`,
            EnumType, id.stringof, id.stringof, ArgsToString!Args
        );
    }
}

import nijilive;
//import nijigenerate.core;
import nijigenerate.ext;

struct TW(alias T, string fieldName, string fieldDesc) {}

class Context {
    Puppet _puppet;
    Node[] _nodes;
    Parameter[] _parameters;
    ParameterBinding[] _bindings;
    vec2u _keyPoint;
    enum ContextMask {
        None = 0,
        HasPuppet = 1,
        HasNodes = 2,
        HasParameters = 4,
        HasBindings = 8,
        HasKeyPoint = 16,
    }
    ContextMask masks = ContextMask.None;
    bool hasPuppet()    { return (masks & ContextMask.HasPuppet)    != 0; }
    bool hasNodes()     { return (masks & ContextMask.HasNodes)     != 0; }
    bool hasParameters(){ return (masks & ContextMask.HasParameters) != 0; }
    bool hasBindings()  { return (masks & ContextMask.HasBindings)  != 0; }
    bool hasKeyPoint()  { return (masks & ContextMask.HasKeyPoint)  != 0; }
    void hasPuppet(bool value)    { masks = value ? (masks | ContextMask.HasPuppet)    : (masks & ~ContextMask.HasPuppet); }
    void hasNodes(bool value)     { masks = value ? (masks | ContextMask.HasNodes)     : (masks & ~ContextMask.HasNodes); }
    void hasParameters(bool value){ masks = value ? (masks | ContextMask.HasParameters): (masks & ~ContextMask.HasParameters); }
    void hasBindings(bool value)  { masks = value ? (masks | ContextMask.HasBindings)  : (masks & ~ContextMask.HasBindings); }
    void hasKeyPoint(bool value)  { masks = value ? (masks | ContextMask.HasKeyPoint)  : (masks & ~ContextMask.HasKeyPoint); }

    Puppet puppet() { return _puppet; }
    void puppet(Puppet value) { _puppet = value; hasPuppet = true; }

    Node[] nodes() { return _nodes; }
    void nodes(Node[] value) { _nodes = value; hasNodes = true; }

    Parameter[] parameters() { return _parameters; }
    void parameters(Parameter[] value) { _parameters = value; hasParameters = true; }

    ParameterBinding[] bindings() { return _bindings; }
    void bindings(ParameterBinding[] value) { _bindings = value; hasBindings = true; }

    vec2u keyPoint() { return _keyPoint; }
    void keyPoint(vec2u value) { _keyPoint = value; hasKeyPoint = true; }
}

interface Command {
    void run(Context context);
    string label();        // short display name for menus, i18n-applied at call site
    string description();  // longer human description
    /// Whether this command can run in the given context
    bool runnable(Context context);
}

abstract class ExCommand(T...) : Command {
    template _unwrapType(W) {
        static if (is(W == TW!(E, fname, fdesc), alias E, string fname, string fdesc))
            alias _unwrapType = E;
        else
            alias _unwrapType = W;
    }
    template unwrapTypes(WTs...) {
        alias unwrapTypes = staticMap!(_unwrapType, WTs);
    }
    template generateMembers(U...) {
        enum generateMembers = generateMembersImpl!(0, U);
    }
    template generateMembersImpl(size_t idx, U...) {
        static if (U.length == 0) {
            enum string generateMembersImpl = "";
        } else {
            alias AParam = U[0];
            static if (isInstanceOf!(TW, AParam)) {
                enum string thisDecl = fullyQualifiedName!(TemplateArgsOf!AParam[0]) ~ " " ~ TemplateArgsOf!AParam[1] ~ ";\n";
            } else {
                enum string thisDecl = AParam.stringof ~ " arg" ~ idx.to!string ~ ";\n";
            }
            enum string generateMembersImpl = thisDecl ~ generateMembersImpl!(idx + 1, U[1 .. $]);
        }
    }
    mixin(generateMembers!T);
    string _label;
    string _desc;
    static if (T.length == 0) {
        // Backward-compatible: single-arg constructor sets description only
        this(string desc) { this._desc = desc; this._label = ""; }
        // Preferred: label + description
        this(string label, string desc) { this._label = label; this._desc = desc; }
    } else {
        // Backward-compatible: single-arg (desc) + args
        this(A...)(string desc, A args)
            if (A.length == T.length) {
            this._desc = desc;
            this._label = "";
            static if (A.length != T.length)
                static assert(false, "Expected " ~ T.length.stringof ~ " args, got " ~ A.length.stringof);
            static foreach (i, Param; T) {
                static if (isInstanceOf!(TW, Param)) {
                    mixin("this." ~ TemplateArgsOf!Param[1] ~ " = args[" ~ i.to!string ~ "];");
                } else {
                    mixin("this.arg" ~ i.to!string ~ " = args[" ~ i.to!string ~ "];");
                }
            }
        }
        // Preferred: label + description + args
        this(A...)(string label, string desc, A args)
            if (A.length == T.length) {
            this._label = label;
            this._desc = desc;
            static if (A.length != T.length)
                static assert(false, "Expected " ~ T.length.stringof ~ " args, got " ~ A.length.stringof);
            static foreach (i, Param; T) {
                static if (isInstanceOf!(TW, Param)) {
                    mixin("this." ~ TemplateArgsOf!Param[1] ~ " = args[" ~ i.to!string ~ "];");
                } else {
                    mixin("this.arg" ~ i.to!string ~ " = args[" ~ i.to!string ~ "];");
                }
            }
        }
    }
    override void run(Context context) {}
    override string label() { return _label.length ? _label : _desc; }
    override string description() { return _desc; }
    override bool runnable(Context context) { return true; }
    struct ArgMeta {
        string typeName;
        string fieldName;
        string fieldDesc;
    }
    static ArgMeta[] reflectArgMeta() {
        ArgMeta[] metas;
        static foreach (Param; T) {
            static if (isInstanceOf!(TW, Param)) {
                metas ~= ArgMeta(
                    TemplateArgsOf!Param[0].stringof,
                    TemplateArgsOf!Param[1],
                    TemplateArgsOf!Param[2]
                );
            }
        }
        return metas;
    }
}

// ===== Helper utilities for menu integration =====
import bindbc.imgui;
import i18n;
import std.string : toStringz;
import std.traits : isInstanceOf, BaseClassesTuple, TemplateArgsOf;

// Resolve pre-registered command instance by enum id
private Command _resolveCommandInstance(alias id)()
{
    // Access the commands map via a local MapByKeyType clone and AllCommandMaps
    import nijigenerate.commands : AllCommandMaps; // public alias to all maps

    // Local AA helpers (duplicated to avoid relying on private templates)
    template KeyTypeOfAA(alias AA)
    {
        static if (is(typeof(AA) : V[K], V, K))
            alias KeyTypeOfAA = K;
        else
            static assert(0, AA.stringof ~ " is not an associative array");
    }
    template MapByKeyType(K, Maps...)
    {
        static if (Maps.length == 0)
            static assert(0, "No commands map found for key type: " ~ K.stringof);
        else static if (is(KeyTypeOfAA!(Maps[0]) == K))
            alias MapByKeyType = Maps[0];
        else
            alias MapByKeyType = MapByKeyType!(K, Maps[1 .. $]);
    }

    alias CmdsAA = MapByKeyType!(typeof(id), AllCommandMaps);
    auto p = id in CmdsAA;
    enforce(p !is null, "No registered command for id: " ~ id.stringof ~ " (key type: " ~ typeof(id).stringof ~ ")");
    return *p;
}

// Extract the template argument list (the declared parameter types) from a concrete ExCommand subclass
private template _BaseExArgsOf(alias C)
{
    alias _Bases = BaseClassesTuple!C;

    private template _FindExArgs(Bases...)
    {
        static if (Bases.length == 0) {
            alias _FindExArgs = void;
        } else static if (isInstanceOf!(ExCommand, Bases[0])) {
            alias _FindExArgs = TemplateArgsOf!(Bases[0]);
        } else {
            alias _FindExArgs = _FindExArgs!(Bases[1 .. $]);
        }
    }

    alias _Picked = _FindExArgs!(_Bases);
    static if (is(_Picked == void)) {
        static assert(0, C.stringof ~ " is not derived from ExCommand!T");
    } else {
        alias _BaseExArgsOf = _Picked;
    }
}

// Apply passed args to the instance fields declared by ExCommand!T before run(ctx)
private void _applyArgsFor(alias C, A...)(C inst, auto ref A args)
{
    alias Declared = _BaseExArgsOf!C; // may include TW!(T, name, desc)

    static if (A.length == 0) {
        // nothing to apply
    } else {
        static assert(A.length == Declared.length,
            C.stringof ~ ": expected " ~ Declared.length.stringof ~ " args, got " ~ A.length.stringof);

        static foreach (i, Param; Declared) {
            static if (isInstanceOf!(TW, Param)) {
                mixin("alias TParam"~i.stringof~" = TemplateArgsOf!Param[0];");
                mixin("enum fname"~i.stringof~ "= TemplateArgsOf!Param[1];");
                static assert(is(typeof(args[i]) : mixin("TParam"~i.stringof)),
                    C.stringof ~ ": argument #" ~ i.stringof ~ " type mismatch: got " ~ typeof(args[i]).stringof ~ ", expected " ~ mixin("TParam"~i.stringof).stringof);
                mixin("inst." ~ mixin("fname"~i.stringof) ~ " = args[" ~ i.to!string ~ "]; ");
            } else {
                mixin("alias TParam"~i.stringof~ "= Param;");
                static assert(is(typeof(args[i]) : mixin("TParam"~i.stringof)),
                    C.stringof ~ ": argument #" ~ i.stringof ~ " type mismatch: got " ~ typeof(args[i]).stringof ~ ", expected " ~ mixin("TParam"~i.stringof).stringof);
                mixin("enum fname"~i.stringof~" = \"arg\" ~ i.to!string;");
                mixin("inst." ~ mixin("fname"~i.stringof) ~ " = args[" ~ i.to!string ~ "]; ");
            }
        }
    }
}

// Draw a single ImGui menu item using command's label and registered shortcut (if any),
// then invoke the command when selected. Returns the concrete command instance or null.
// No-arg variant keeps backward-compatible selected/enabled defaults
Command ngMenuItemFor(alias id)(ref Context ctx, bool selected = false, bool enabled = true)
{
    auto base = _resolveCommandInstance!id();
    if (base is null) return null;

    import nijigenerate.core.shortcut : ngShortcutFor;
    auto shortcut = ngShortcutFor(base);
    const(char)* pShortcut = shortcut.length ? shortcut.toStringz : null;
    auto lbl = base.label();
    bool canRun = base.runnable(ctx);
    bool enabledFinal = enabled && canRun;
    if (igMenuItem(__(lbl), pShortcut, selected, enabledFinal)) {
        import nijigenerate.commands : cmd;
        cmd!id(ctx);
    }
    return base;
}

// With-args variant requires explicit selected/enabled (no defaults)
Command ngMenuItemFor(alias id, A...)(ref Context ctx, bool selected, bool enabled, auto ref A args)
    if (A.length > 0)
{
    auto base = _resolveCommandInstance!id();
    if (base is null) return null;

    import nijigenerate.core.shortcut : ngShortcutFor;
    auto shortcut = ngShortcutFor(base);
    const(char)* pShortcut = shortcut.length ? shortcut.toStringz : null;
    auto lbl = base.label();
    bool canRun = base.runnable(ctx);
    bool enabledFinal = enabled && canRun;
    if (igMenuItem(__(lbl), pShortcut, selected, enabledFinal)) {
        import nijigenerate.commands : cmd;
        cmd!id(ctx, args);
    }
    return base;
}
