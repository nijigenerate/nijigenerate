module nijigenerate.commands.parameter.group;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijilive;
import nijigenerate.ext;
import nijigenerate.ext.param;
import nijigenerate.widgets;
import nijigenerate.windows;
import nijigenerate.core;
import nijigenerate.project;
import i18n;
import std.array : insertInPlace;


//==================================================================================
// Command Palette Definition for Parameter Group
//==================================================================================


class MoveParameterCommand : ExCommand!(TW!(ExParameterGroup,"group",""), TW!(int, "index", "")) {
    this(ExParameterGroup group, int index) { super(null, "Move Parameter", group, index);}
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return;

        incMoveParameter(ctx.parameters[0], group, index);
    }
}

class CreateParamGroupCommand : ExCommand!(TW!(int, "index", "")) {
    this(int index = 0) { super(null, "Create Parameter Group", index); }
    override
    void run(Context ctx) {

        if (!ctx.hasPuppet) return;

//        if (index < 0) index = 0;
//        else if (index > ctx.puppet.parameters.length) 
//            index = cast(int)ctx.puppet.parameters.length-1;

        auto group = new ExParameterGroup(_("New Parameter Group"));
        (cast(ExPuppet)ctx.puppet).addGroup(group);
    }
}

class ChangeGroupColorCommand : ExCommand!(TW!(float[3], "color", "color value for target Parameter Group.")) {
    this(float[3] color = [0f, 0f, 0f]) { super(null, "Change Parameter Group Color", color); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length < 1 || (cast(ExParameterGroup)ctx.parameters[0]) is null)
            return;
        auto group = cast(ExParameterGroup)ctx.parameters[0];
        group.color = vec3(color[0], color[1], color[2]);
    }
}

class DeleteParamGroupCommand : ExCommand!() {
    this() { super(null, "Delete Parameter Group"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length < 1 || (cast(ExParameterGroup)ctx.parameters[0]) is null)
            return;
        auto group = cast(ExParameterGroup)ctx.parameters[0];

        foreach(child; group.children) {
            auto exChild = cast(ExParameter)child;
            exChild.setParent(null);
        }
        (cast(ExPuppet)incActivePuppet()).removeGroup(group);
    }
}

enum GroupCommand {
    MoveParameter,
    CreateParamGroup,
    ChangeGroupColor,
    DeleteParamGroup
}

Command[GroupCommand] commands;

void ngInitCommands(T)() if (is(T == GroupCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!GroupCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    mixin(registerCommand!(GroupCommand.MoveParameter, null, 0));
    mixin(registerCommand!(GroupCommand.CreateParamGroup, 0));
    mixin(registerCommand!(GroupCommand.ChangeGroupColor, [0f, 0f, 0f]));
}
