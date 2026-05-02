module nijigenerate.commands.base;

import std.traits : isInstanceOf, TemplateArgsOf, BaseClassesTuple, fullyQualifiedName, EnumMembers;
import std.meta : staticMap, AliasSeq;
import std.array : join;
import std.format : format;
import std.conv : to;
import std.exception : enforce;
static import std.json;
import std.json : JSONValue;
import nijigenerate.panels.inspector.common;
static import nijigenerate.viewport.common.mesheditor.tools.enums;
static import nijilive.core.nodes.drivers; // PhysicsModel, ParamMapMode, Driver
static import nijilive.core.nodes.drivers.simplephysics;

string toCodeString(T)(T arg) {
    import std.traits : isSomeString, isIntegral, isFloatingPoint;
    import std.conv : text;
    static if (isSomeString!T) {
        import std.string : replace;
        return `"` ~ (cast(string)arg).replace(`"`, `\"`) ~ `"`;
    } else static if (is(T == typeof(null))) {
        return "null";
    } else static if (is(T == enum)) {
        return T.stringof ~ "." ~ __traits(identifier, arg);
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
        enum registerCommand = format(`{ auto __cmd = new %3$sCommand(); ngRegisterCommandMeta(__cmd); commands[%1$s.%2$s] = __cmd; }`, EnumType, id.stringof, id.stringof);
    } else {
        enum registerCommand = format(
            `{ auto __cmd = new %3$sCommand(%4$s); ngRegisterCommandMeta(__cmd); commands[%1$s.%2$s] = __cmd; }`,
            EnumType, id.stringof, id.stringof, ArgsToString!Args
        );
    }
}

import nijilive;
//import nijigenerate.core;
import nijigenerate.ext;
struct TW(alias T, string fieldName, string fieldDesc, bool hidden = false) {}

enum CommandShortcutPolicy {
    visible,
    hidden,
}

enum CommandMcpExposure {
    tool,
    hidden,
}

enum CommandGuiDisplay {
    none,
    window,
    dialog,
    modal,
    popup,
    confirm,
    dialogOutput,
    dialogWindow,
    layout,
}

enum CommandIrreversibleEffect {
    none,
    fileWrite,
    importData,
    projectReset,
    configEdit,
    apply,
    keyframeEdit,
    bindingEdit,
    structuralEdit,
    create,
    deleteObject,
    rename,
    textureRegenerate,
    repair,
    layoutReset,
}

struct ShortcutVisibility {
    CommandShortcutPolicy value;
}

struct McpExposureMeta {
    CommandMcpExposure value;
}

struct GuiDisplayMeta {
    CommandGuiDisplay value;
}

struct IrreversibleEffectMeta {
    CommandIrreversibleEffect value;
}

enum ShortcutVisible = ShortcutVisibility(CommandShortcutPolicy.visible);
enum ShortcutHidden = ShortcutVisibility(CommandShortcutPolicy.hidden);
enum McpTool = McpExposureMeta(CommandMcpExposure.tool);
enum McpHidden = McpExposureMeta(CommandMcpExposure.hidden);

enum GuiWindow = GuiDisplayMeta(CommandGuiDisplay.window);
enum GuiDialog = GuiDisplayMeta(CommandGuiDisplay.dialog);
enum GuiModal = GuiDisplayMeta(CommandGuiDisplay.modal);
enum GuiPopup = GuiDisplayMeta(CommandGuiDisplay.popup);
enum GuiConfirm = GuiDisplayMeta(CommandGuiDisplay.confirm);
enum GuiDialogOutput = GuiDisplayMeta(CommandGuiDisplay.dialogOutput);
enum GuiDialogWindow = GuiDisplayMeta(CommandGuiDisplay.dialogWindow);
enum GuiLayout = GuiDisplayMeta(CommandGuiDisplay.layout);

enum EffectFileWrite = IrreversibleEffectMeta(CommandIrreversibleEffect.fileWrite);
enum EffectImport = IrreversibleEffectMeta(CommandIrreversibleEffect.importData);
enum EffectProjectReset = IrreversibleEffectMeta(CommandIrreversibleEffect.projectReset);
enum EffectConfigEdit = IrreversibleEffectMeta(CommandIrreversibleEffect.configEdit);
enum EffectApply = IrreversibleEffectMeta(CommandIrreversibleEffect.apply);
enum EffectKeyframeEdit = IrreversibleEffectMeta(CommandIrreversibleEffect.keyframeEdit);
enum EffectBindingEdit = IrreversibleEffectMeta(CommandIrreversibleEffect.bindingEdit);
enum EffectStructuralEdit = IrreversibleEffectMeta(CommandIrreversibleEffect.structuralEdit);
enum EffectCreate = IrreversibleEffectMeta(CommandIrreversibleEffect.create);
enum EffectDelete = IrreversibleEffectMeta(CommandIrreversibleEffect.deleteObject);
enum EffectRename = IrreversibleEffectMeta(CommandIrreversibleEffect.rename);
enum EffectTextureRegenerate = IrreversibleEffectMeta(CommandIrreversibleEffect.textureRegenerate);
enum EffectRepair = IrreversibleEffectMeta(CommandIrreversibleEffect.repair);
enum EffectLayoutReset = IrreversibleEffectMeta(CommandIrreversibleEffect.layoutReset);

struct CommandMetadata {
    bool shortcutRunnable = true;
    bool mcpExposed = true;
    CommandGuiDisplay guiDisplay = CommandGuiDisplay.none;
    CommandIrreversibleEffect irreversibleEffect = CommandIrreversibleEffect.none;
}

private __gshared CommandMetadata[string] gCommandMetadataByTypeName;

private CommandShortcutPolicy _shortcutPolicyOf(C)() {
    static foreach (attr; __traits(getAttributes, C)) {
        static if (is(typeof(attr) == ShortcutVisibility)) {
            return attr.value;
        }
    }
    return CommandShortcutPolicy.visible;
}

private CommandMcpExposure _mcpExposureOf(C)() {
    static foreach (attr; __traits(getAttributes, C)) {
        static if (is(typeof(attr) == McpExposureMeta)) {
            return attr.value;
        }
    }
    return CommandMcpExposure.tool;
}

private CommandGuiDisplay _guiDisplayOf(C)() {
    static foreach (attr; __traits(getAttributes, C)) {
        static if (is(typeof(attr) == GuiDisplayMeta)) {
            return attr.value;
        }
    }
    return CommandGuiDisplay.none;
}

private CommandIrreversibleEffect _irreversibleEffectOf(C)() {
    static foreach (attr; __traits(getAttributes, C)) {
        static if (is(typeof(attr) == IrreversibleEffectMeta)) {
            return attr.value;
        }
    }
    return CommandIrreversibleEffect.none;
}

void ngRegisterCommandMeta(C)(C cmd) if (is(C : Command)) {
    auto key = typeid(cast(Object)cmd).toString();
    gCommandMetadataByTypeName[key] = CommandMetadata(
        _shortcutPolicyOf!C() != CommandShortcutPolicy.hidden,
        _mcpExposureOf!C() != CommandMcpExposure.hidden,
        _guiDisplayOf!C(),
        _irreversibleEffectOf!C()
    );
}

private CommandMetadata ngLookupCommandMeta(Command cmd) {
    auto key = typeid(cast(Object)cmd).toString();
    if (auto p = key in gCommandMetadataByTypeName) {
        return *p;
    }
    return CommandMetadata.init;
}

// Append enum choices to description for UI/MCP exposure
string enrichArgDesc(T)(string baseDesc) {
    static if (is(T == enum)) {
        string s;
        bool first = true;
        foreach (mem; EnumMembers!T) {
            if (!first) s ~= "|";
            first = false;
            s ~= __traits(identifier, mem);
            static if (__traits(compiles, cast(string)mem)) {
                enum string memValue = cast(string)mem;
                static if (memValue != __traits(identifier, mem)) {
                    s ~= "=" ~ memValue;
                }
            }
        }
        auto choices = "(" ~ s ~ ")";
        return baseDesc.length ? (baseDesc ~ " " ~ choices) : choices;
    } else {
        return baseDesc;
    }
}

class CommandResult {
    bool succeeded;
    string message;
    this(bool succeeded, string message = "") {
        this.succeeded = succeeded;
        this.message = message;
    }
    static CommandResult opCall(bool succeeded, string message = "") {
        return new CommandResult(succeeded, message);
    }
    CommandResult waitForCompletion() {
        return this;
    }
}

// If T is already a CommandResult (e.g., CreateResult!R), inherit from it directly.
static if (is(T : CommandResult))
class ExCommandResult(T) : T {
    this(Args...)(Args args) if (__traits(compiles, { super(args); })) { super(args); }
    static ExCommandResult!T opCall(Args...)(Args args) if (__traits(compiles, { return new ExCommandResult!T(args); })) {
        return new ExCommandResult!T(args);
    }
}
else
// General ExCommandResult: if T is already a CommandResult, just alias to T.
template ExCommandResult(T) {
    static if (is(T : CommandResult)) {
        alias ExCommandResult = T;
    } else {
        alias ExCommandResult = ExCommandResultImpl!T;
    }
}

// Payload-carrying ExCommandResult for non-CommandResult types
class ExCommandResultImpl(T) : CommandResult {
    T result;
    this(bool succeeded, T result = T.init, string message = "") {
        super(succeeded, message);
        this.result = result;
    }
    static ExCommandResultImpl!T opCall(bool succeeded, T result = T.init, string message = "") {
        return new ExCommandResultImpl!T(succeeded, result, message);
    }
}

class CreateResult(R) : CommandResult {
    R[] created;
    this(bool succeeded, R[] created = null, string message = "") {
        super(succeeded, message);
        if (created !is null) this.created = created;
    }
    static CreateResult!R opCall(bool succeeded, R[] created = null, string message = "") {
        return new CreateResult!R(succeeded, created, message);
    }
}

class DeleteResult(R) : CommandResult {
    R[] deleted;
    this(bool succeeded, R[] deleted = null, string message = "") {
        super(succeeded, message);
        if (deleted !is null) this.deleted = deleted;
    }
    static DeleteResult!R opCall(bool succeeded, R[] deleted = null, string message = "") {
        return new DeleteResult!R(succeeded, deleted, message);
    }
}

class LoadResult(R) : CommandResult {
    R[] loaded;
    this(bool succeeded, R[] loaded = null, string message = "") {
        super(succeeded, message);
        if (loaded !is null) this.loaded = loaded;
    }
    static LoadResult!R opCall(bool succeeded, R[] loaded = null, string message = "") {
        return new LoadResult!R(succeeded, loaded, message);
    }
}

class Context {
    Puppet _puppet;
    Node[] _nodes;
    Parameter[] _parameters;
    Parameter[] _armedParameters;
    ParameterBinding[] _bindings;
    ParameterBinding[] _activeBindings;
    vec2u _keyPoint;
    vec2 _parameterValue;
    TypedInspector!Node[] _inspectors; // preferred: list of active inspectors
    enum ContextMask {
        None = 0,
        HasPuppet = 1,
        HasNodes = 2,
        HasParameters = 4,
        HasBindings = 8,
        HasKeyPoint = 16,
        HasInspectors = 32,
        HasArmedParameters = 64,
        HasActiveBindings = 128,
        HasExplicitKeyPoint = 256,
        HasParameterValue = 512,
    }
    ContextMask masks = ContextMask.None;
    bool hasPuppet()    { return (masks & ContextMask.HasPuppet)    != 0; }
    bool hasNodes()     { return (masks & ContextMask.HasNodes)     != 0; }
    bool hasParameters(){ return (masks & ContextMask.HasParameters) != 0; }
    bool hasArmedParameters() { return (masks & ContextMask.HasArmedParameters) != 0; }
    bool hasBindings()  { return (masks & ContextMask.HasBindings)  != 0; }
    bool hasKeyPoint()  { return (masks & ContextMask.HasKeyPoint)  != 0; }
    bool hasInspectors() { return (masks & ContextMask.HasInspectors) != 0; }
    bool hasActiveBindings() { return (masks & ContextMask.HasActiveBindings) != 0; }
    bool hasExplicitKeyPoint() { return (masks & ContextMask.HasExplicitKeyPoint) != 0; }
    bool hasParameterValue() { return (masks & ContextMask.HasParameterValue) != 0; }
    void hasPuppet(bool value)    { masks = value ? (masks | ContextMask.HasPuppet)    : (masks & ~ContextMask.HasPuppet); }
    void hasNodes(bool value)     { masks = value ? (masks | ContextMask.HasNodes)     : (masks & ~ContextMask.HasNodes); }
    void hasParameters(bool value){ masks = value ? (masks | ContextMask.HasParameters): (masks & ~ContextMask.HasParameters); }
    void hasArmedParameters(bool value) { masks = value ? (masks | ContextMask.HasArmedParameters) : (masks & ~ContextMask.HasArmedParameters); }
    void hasBindings(bool value)  { masks = value ? (masks | ContextMask.HasBindings)  : (masks & ~ContextMask.HasBindings); }
    void hasKeyPoint(bool value)  { masks = value ? (masks | ContextMask.HasKeyPoint)  : (masks & ~ContextMask.HasKeyPoint); }
    void hasInspectors(bool value) { masks = value ? (masks | ContextMask.HasInspectors)  : (masks & ~ContextMask.HasInspectors); }
    void hasActiveBindings(bool value) { masks = value ? (masks | ContextMask.HasActiveBindings) : (masks & ~ContextMask.HasActiveBindings); }
    void hasExplicitKeyPoint(bool value) { masks = value ? (masks | ContextMask.HasExplicitKeyPoint) : (masks & ~ContextMask.HasExplicitKeyPoint); }
    void hasParameterValue(bool value) { masks = value ? (masks | ContextMask.HasParameterValue) : (masks & ~ContextMask.HasParameterValue); }

    Puppet puppet() { return _puppet; }
    void puppet(Puppet value) { _puppet = value; hasPuppet = true; }

    Node[] nodes() { return _nodes; }
    void nodes(Node[] value) { _nodes = value; hasNodes = true; }

    Parameter[] parameters() { return _parameters; }
    void parameters(Parameter[] value) { _parameters = value; hasParameters = true; }

    Parameter[] armedParameters() { return _armedParameters; }
    void armedParameters(Parameter[] value) { _armedParameters = value; hasArmedParameters = true; }

    ParameterBinding[] bindings() { return _bindings; }
    void bindings(ParameterBinding[] value) { _bindings = value; hasBindings = true; }

    vec2u keyPoint() { return _keyPoint; }
    void keyPoint(vec2u value) { _keyPoint = value; hasKeyPoint = true; }

    vec2 parameterValue() { return _parameterValue; }
    void parameterValue(vec2 value) { _parameterValue = value; hasParameterValue = true; }

    // Preferred: list of inspector instances
    TypedInspector!Node[] inspectors() { return _inspectors; }
    void inspectors(TypedInspector!Node[] value) { _inspectors = value; hasInspectors = true; }

    ParameterBinding[] activeBindings() { return _activeBindings; }
    void activeBindings(ParameterBinding[] value) { _activeBindings = value; hasActiveBindings = true; }
}

interface Command {
    CommandResult run(Context context);
    string label();        // short display name for menus, i18n-applied at call site
    string description();  // longer human description
    /// Whether this command can run in the given context
    bool runnable(Context context);
    /// Whether this command is eligible to be bound and edited as a shortcut
    bool shortcutRunnable();
    /// Whether this command should be exposed via MCP tools
    bool mcpExposed();
    /// Direct GUI display style triggered by this command
    CommandGuiDisplay guiDisplay();
    /// Persistent or one-way side effect category
    CommandIrreversibleEffect irreversibleEffect();
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
        this(string label, string desc) { 
            this._label = label ? label: desc; 
            this._desc = desc; 
        }
    } else {
        // Allow derived classes to set labels/fields without passing args
        this() {}
        // Optional: label + description only (fields left default-initialized)
        this(string label, string desc) { this._label = label; this._desc = desc; }
        // Preferred: label + description + args
        this(A...)(string label, string desc, A args)
            if (A.length == T.length) {
            if (label !is null)
                this._label = label;
            else
                this._label = desc;
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
    override CommandResult run(Context context) { return CommandResult(true); }
    override string label() { return _label.length ? _label : _desc; }
    override string description() { return _desc; }
    override bool runnable(Context context) { return true; }
    override bool shortcutRunnable() { return ngLookupCommandMeta(this).shortcutRunnable; }
    override bool mcpExposed() { return ngLookupCommandMeta(this).mcpExposed; }
    override CommandGuiDisplay guiDisplay() { return ngLookupCommandMeta(this).guiDisplay; }
    override CommandIrreversibleEffect irreversibleEffect() { return ngLookupCommandMeta(this).irreversibleEffect; }
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

// Build a human-facing, stable identifier for a command-map key.
// This string is used in user-visible listings (e.g., Command Browser) and persistence (e.g., shortcuts).
string ngCommandIdFromKey(K)(K k)
{
    import std.conv : to;
    import std.string : startsWith;
    static import nodeDynamic = nijigenerate.commands.node.dynamic;
    static import autoMeshDynamic = nijigenerate.commands.automesh.dynamic;
    static import autoMeshConfig = nijigenerate.commands.automesh.config;
    static import inspectorApply = nijigenerate.commands.inspector.apply_node;
    static import panelView = nijigenerate.commands.view.panel;

    string stripPrefix(string value, string prefix) {
        return value.startsWith(prefix) ? value[prefix.length .. $] : value;
    }

    string stripSuffix(string value, string suffix) {
        return value.length >= suffix.length && value[$ - suffix.length .. $] == suffix
            ? value[0 .. $ - suffix.length]
            : value;
    }

    string lowerLeading(string value) {
        import std.ascii : toLower;
        auto r = value.dup;
        if (r.length) {
            r[0] = toLower(r[0]);
        }
        return cast(string)r;
    }

    static if (is(K == nodeDynamic.AddNodeKey)) {
        return "Node.Add." ~ to!string(k);
    } else static if (is(K == nodeDynamic.InsertNodeKey)) {
        return "Node.Insert." ~ to!string(k);
    } else static if (is(K == nodeDynamic.ConvertToKey)) {
        return "Node.ConvertTo." ~ to!string(k);
    } else static if (is(K == autoMeshDynamic.AutoMeshKey)) {
        return "AutoMesh.Apply." ~ to!string(k);
    } else static if (is(K == autoMeshConfig.AutoMeshConfigCommand)) {
        return "AutoMesh." ~ stripPrefix(to!string(k), "AutoMesh");
    } else static if (is(K == autoMeshConfig.AutoMeshGetConfigKey)) {
        return "AutoMesh.GetConfig." ~ k.id;
    } else static if (is(K == autoMeshConfig.AutoMeshSetConfigKey)) {
        return "AutoMesh.SetConfig." ~ k.id;
    } else static if (is(K == autoMeshConfig.AutoMeshSetSimpleKey)) {
        return "AutoMesh.SetSimple." ~ k.id;
    } else static if (is(K == autoMeshConfig.AutoMeshSetAdvancedKey)) {
        return "AutoMesh.SetAdvanced." ~ k.id;
    } else static if (is(K == autoMeshConfig.AutoMeshSetPresetKey)) {
        return "AutoMesh.SetPreset." ~ k.id;
    } else static if (is(K == autoMeshConfig.AutoMeshTypedCommand)) {
        auto name = to!string(k);
        if (name.startsWith("AutoMeshSetSimple_")) {
            auto proc = stripPrefix(name, "AutoMeshSetSimple_");
            return "AutoMesh.SetSimple." ~ lowerLeading(stripSuffix(proc, "AutoMeshProcessor"));
        } else if (name.startsWith("AutoMeshSetAdvanced_")) {
            auto proc = stripPrefix(name, "AutoMeshSetAdvanced_");
            return "AutoMesh.SetAdvanced." ~ lowerLeading(stripSuffix(proc, "AutoMeshProcessor"));
        } else if (name.startsWith("AutoMeshSetPreset_")) {
            auto proc = stripPrefix(name, "AutoMeshSetPreset_");
            return "AutoMesh.SetPreset." ~ lowerLeading(stripSuffix(proc, "AutoMeshProcessor"));
        }
        return "AutoMesh." ~ name;
    } else static if (is(K == inspectorApply.InspectorNodeApplyCommand)) {
        return "Inspector.Apply." ~ to!string(k);
    } else static if (is(K == panelView.PanelKey)) {
        return "Panel.Toggle." ~ to!string(k);
    } else static if (K.stringof == "AnimeditCommand") {
        return "Animation.Add" ~ stripPrefix(to!string(k), "AddAnimation");
    } else {
        return typeof(k).stringof ~ "." ~ to!string(k);
    }
}

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

// Public alias for reuse outside this module
alias BaseExArgsOf = _BaseExArgsOf;

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
