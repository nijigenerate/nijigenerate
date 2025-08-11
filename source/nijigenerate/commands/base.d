module nijigenerate.commands.base;

import std.traits : isInstanceOf, TemplateArgsOf, BaseClassesTuple;
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
import nijigenerate.core;
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
    string desc();
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
                enum string thisDecl = TemplateArgsOf!AParam[0].stringof ~ " " ~ TemplateArgsOf!AParam[1] ~ ";\n";
            } else {
                enum string thisDecl = AParam.stringof ~ " arg" ~ idx.to!string ~ ";\n";
            }
            enum string generateMembersImpl = thisDecl ~ generateMembersImpl!(idx + 1, U[1 .. $]);
        }
    }
    mixin(generateMembers!T);
    private string _desc;
    static if (T.length == 0) {
        this(string desc) { this._desc = desc; }
    } else {
        this(A...)(string desc, A args) {
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
    override string desc() { return _desc; }
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