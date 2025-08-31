module nijigenerate.commands;
public import nijigenerate.commands.base;
public import nijigenerate.commands.binding.binding;
public import nijigenerate.commands.node.node;
public import nijigenerate.commands.node.dynamic;
public import nijigenerate.commands.parameter.animedit;
public import nijigenerate.commands.parameter.group;
public import nijigenerate.commands.parameter.param;
public import nijigenerate.commands.parameter.paramedit;
public import nijigenerate.commands.parameter.prop;
public import nijigenerate.commands.puppet.file;
public import nijigenerate.commands.puppet.edit;
public import nijigenerate.commands.puppet.view;
public import nijigenerate.commands.puppet.tool;
public import nijigenerate.commands.viewport.control;
public import nijigenerate.commands.viewport.palette;
public import nijigenerate.commands.mesheditor.tool;
public import nijigenerate.commands.view.panel;
public import nijigenerate.commands.inspector.apply_node;

import std.meta : AliasSeq;
import std.traits : BaseClassesTuple, isInstanceOf, TemplateArgsOf;
import std.exception : enforce;
import std.conv;

alias AllCommandMaps = AliasSeq!(
    nijigenerate.commands.binding.binding.commands,
    nijigenerate.commands.node.node.commands,
    nijigenerate.commands.parameter.animedit.commands,
    nijigenerate.commands.parameter.group.commands,
    nijigenerate.commands.parameter.param.commands,
    nijigenerate.commands.parameter.paramedit.commands,
    nijigenerate.commands.parameter.prop.commands,
    nijigenerate.commands.puppet.file.commands,
    nijigenerate.commands.puppet.edit.commands,
    nijigenerate.commands.puppet.view.commands,
    nijigenerate.commands.puppet.tool.commands,
    nijigenerate.commands.viewport.control.commands,
    nijigenerate.commands.viewport.palette.commands,
    nijigenerate.commands.mesheditor.tool.selectToolModeCommands,
    nijigenerate.commands.view.panel.togglePanelCommands,
    nijigenerate.commands.node.dynamic.addNodeCommands,
    nijigenerate.commands.node.dynamic.insertNodeCommands,
    nijigenerate.commands.node.dynamic.convertNodeCommands,
    nijigenerate.commands.inspector.apply_node.commands,
    );

// Explicit initialization to avoid module constructor cycles
// Discover and initialize commands for each enum key present in AllCommandMaps.
void ngInitAllCommands() {
    static foreach (AA; AllCommandMaps) {
        import std.stdio;
        writef("niInitCoomands: %s", KeyTypeOfAA!(AA).stringof);
        static if (__traits(compiles, { ngInitCommands!(KeyTypeOfAA!(AA))(); })) {
            writefln("Calls");
            ngInitCommands!(KeyTypeOfAA!(AA))();
        } else writefln("NOt called");
    }
    /*
    import std.stdio;
    import std.conv;
    foreach (cmds; AllCommandMaps) {
        // cmds は連想配列(enumType => valueType)
        foreach (k, v; cmds) {
            writeln("[", typeof(k).stringof, "] ", k.to!string, " => ", v);
        }
    }
    */

}

// === cmd!(ID)(ctx, ...) infrastructure ===

// Deduce AA key type from a commands map (V[K])
private template KeyTypeOfAA(alias AA) {
    static if (is(typeof(AA) : V[K], V, K))
        alias KeyTypeOfAA = K;
    else
        static assert(0, AA.stringof ~ " is not an associative array");
}

// Pick the commands map whose key type matches K
private template MapByKeyType(K, Maps...) {
    static if (Maps.length == 0)
        static assert(0, "No commands map found for key type: " ~ K.stringof);
    else static if (is(KeyTypeOfAA!(Maps[0]) == K))
        alias MapByKeyType = Maps[0];
    else
        alias MapByKeyType = MapByKeyType!(K, Maps[1 .. $]);
}

// Extract the template argument list (the declared parameter types) from a concrete ExCommand subclass
private template BaseExArgsOf(alias C) {
    alias _Bases = BaseClassesTuple!C;

    // Find the first base that matches ExCommand!T and return its TemplateArgsOf!B
    private template _FindExArgs(Bases...)
    {
        static if (Bases.length == 0) {
            alias _FindExArgs = void;
        } else static if (isInstanceOf!(ExCommand, Bases[0])) {
            alias _FindExArgs = TemplateArgsOf!(Bases[0]); // may contain TW wrappers
        } else {
            alias _FindExArgs = _FindExArgs!(Bases[1 .. $]);
        }
    }

    alias _Picked = _FindExArgs!(_Bases);
    static if (is(_Picked == void)) {
        static assert(0, C.stringof ~ " is not derived from ExCommand!T");
    } else {
        alias BaseExArgsOf = _Picked;
    }
}

private template _CommandByIdImpl(alias id, Cmds...) {
    static if (Cmds.length == 0) {
        static assert(0, "No registered command type for id: " ~ id.stringof ~ ". Add its class to RegisteredCommands.");
    } else static if (__traits(compiles, Cmds[0].id) && is(typeof(Cmds[0].id) == typeof(id)) && Cmds[0].id == id) {
        alias _CommandByIdImpl = Cmds[0];
    } else {
        alias _CommandByIdImpl = _CommandByIdImpl!(id, Cmds[1 .. $]);
    }
}

// Apply passed args to the instance fields declared by ExCommand!T before run(ctx)
private void _applyArgs(alias C, A...)(C inst, auto ref A args) {
    alias Declared = BaseExArgsOf!C; // may include TW!(T, name, desc)

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

/// Look up a pre-registered command instance from `commands` by enum id,
/// apply passed args to its fields, then run it.
/// Note: `commands` must be an AA like `Command[typeof(id)]` and populated beforehand.
auto cmd(alias id, A...)(ref Context ctx, auto ref A args) {
    // 1) Choose the appropriate commands AA by the id's type, then find the instance (no new)
    alias CmdsAA = MapByKeyType!(typeof(id), AllCommandMaps);
    auto p = id in CmdsAA;
    enforce(p !is null, "No registered command for id: " ~ id.stringof ~ " (key type: " ~ typeof(id).stringof ~ ")");

    Command base = *p;

    // 2) Resolve the concrete command type associated with this id at compile-time
    enum _idName  = __traits(identifier, id);   // e.g., "Add1DParameter"
    enum _typeName = _idName ~ "Command";      // => "Add1DParameterCommand"
    static if (!__traits(compiles, mixin(_typeName))) {
        static assert(0, "No command class found by naming convention: " ~ _typeName ~
                         " (derived from id: " ~ id.stringof ~ ")");
    }
    alias C = mixin(_typeName);

    // 3) Cast the stored instance to its concrete type and apply args (if any)
    auto inst = cast(C) base;
    enforce(inst !is null, "Registered command instance type mismatch for id: " ~ id.stringof ~ ", expected " ~ C.stringof);

    _applyArgs!(C, A)(inst, args);

    // 4) Run and return
    inst.run(ctx);
    return inst;
}
