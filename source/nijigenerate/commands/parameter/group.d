module nijigenerate.commands.parameter.group;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.actions;
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


@EffectStructuralEdit
class MoveParameterCommand : ExCommand!(TW!(ExParameterGroup,"group",""), TW!(int, "index", "")) {
    this(ExParameterGroup group, int index) { super(null, _("Move Parameter"), group, index);}
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0) return CommandResult(false, "No parameters");

        auto exParam = cast(ExParameter)ctx.parameters[0];
        auto oldParent = exParam !is null ? exParam.getParent() : null;
        incMoveParameter(ctx.parameters[0], group, index);
        auto newParent = exParam !is null ? exParam.getParent() : null;
        if (exParam !is null && oldParent !is newParent)
            incActionPush(new ParameterMoveAction(ctx.parameters[0], oldParent, newParent));
        return CommandResult(true);
    }
}

@EffectCreate
class CreateParamGroupCommand : ExCommand!(TW!(int, "index", "")) {
    this(int index = 0) { super(null, _("Create Parameter Group"), index); }
    override
    CreateResult!ExParameterGroup run(Context ctx) {

        if (!ctx.hasPuppet) return new CreateResult!ExParameterGroup(false, null, "No puppet");

//        if (index < 0) index = 0;
//        else if (index > ctx.puppet.parameters.length) 
//            index = cast(int)ctx.puppet.parameters.length-1;

        auto group = new ExParameterGroup(_("New Parameter Group"));
        auto puppet = cast(ExPuppet)ctx.puppet;
        puppet.addGroup(group);
        incActionPush(new ParameterGroupAddAction(puppet, group));
        auto res = new CreateResult!ExParameterGroup(true, [group], "Parameter group created");
        return res;
    }
}

@EffectStructuralEdit
class ChangeGroupColorCommand : ExCommand!(TW!(float[3], "color", "color value for target Parameter Group.")) {
    this(float[3] color = [0f, 0f, 0f]) { super(null, _("Change Parameter Group Color"), color); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length < 1 || (cast(ExParameterGroup)ctx.parameters[0]) is null)
            return CommandResult(false, "No parameter group");
        auto group = cast(ExParameterGroup)ctx.parameters[0];
        auto oldColor = group.color;
        group.color = vec3(color[0], color[1], color[2]);
        incActionPush(new ParameterValueChangeAction!vec3("color", group, oldColor, group.color, &group.color));
        return CommandResult(true);
    }
}

@EffectDelete
class DeleteParamGroupCommand : ExCommand!() {
    this() { super(null, _("Delete Parameter Group")); }
    override
    DeleteResult!ExParameterGroup run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length < 1 || (cast(ExParameterGroup)ctx.parameters[0]) is null)
            return new DeleteResult!ExParameterGroup(false, null, "No parameter group");
        auto group = cast(ExParameterGroup)ctx.parameters[0];
        auto puppet = cast(ExPuppet)incActivePuppet();
        auto action = new ParameterGroupRemoveAction(puppet, group);

        foreach(child; group.children) {
            auto exChild = cast(ExParameter)child;
            exChild.setParent(null);
        }
        puppet.removeGroup(group);
        incActionPush(action);
        auto res = new DeleteResult!ExParameterGroup(true, [group], "Parameter group deleted");
        return res;
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
