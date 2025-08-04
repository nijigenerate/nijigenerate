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


class MoveParameterCommand : ExCommand!(ExParameterGroup, int) {
    this(ExParameterGroup group, int index) { super("Move Parameter", group, index);}
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return;

        incMoveParameter(ctx.parameters[0], arg0, arg1);
    }
}

class CreateParamGroupCommand : ExCommand!(int) {
    this(int index = 0) { super("Create Parameter Group", index); }
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

class ChangeGroupColorCommand : ExCommand!(vec3) {
    this(vec3 color) { super("Change Parameter Group Color", color); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length < 1 || (cast(ExParameterGroup)ctx.parameters[0]) is null)
            return;
        auto group = cast(ExParameterGroup)ctx.parameters[0];
        group.color = arg0;
    }
}

class DeleteParamGroupCommand : ExCommand!() {
    this() { super("Delete Parameter Group"); }
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