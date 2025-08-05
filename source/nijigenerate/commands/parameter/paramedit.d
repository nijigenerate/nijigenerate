module nijigenerate.commands.parameter.paramedit;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.commands.binding.base;
import nijilive;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.viewport.model.deform;
import nijigenerate.ext;
import nijigenerate.project;


//                    incPushWindowList(new ParamPropWindow(param));
//                    incPushWindowList(new ParamAxesWindow(param));
//                    incPushWindowList(new ParamSplitWindow(idx, param));

class ConvertTo2DParamCommand : ExCommand!() {
    this() { super("Convert to 2D"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                convertTo2D(param);
            }
        }
    }
}

class FlipXCommand : ExCommand!() {
    this() { super("Flip X"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                auto action = new ParameterChangeBindingsAction("Flip X", param, null);
                param.reverseAxis(0);
                action.updateNewState();
                incActionPush(action);
            }
        }
    }
}

class FlipYCommand : ExCommand!() {
    this() { super("Flip X"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                auto action = new ParameterChangeBindingsAction("Flip Y", param, null);
                param.reverseAxis(1);
                action.updateNewState();
                incActionPush(action);
            }
        }
    }
}

class Flip1DCommand : ExCommand!() {
    this() { super("Flip"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                auto action = new ParameterChangeBindingsAction("Flip", param, null);
                param.reverseAxis(0);
                action.updateNewState();
                incActionPush(action);
            }
        }
    }
}

class MirrorHorizontallyCommand : ExCommand!() {
    this() { super("Mirror Horizontally"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                mirrorAll(param, 0);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class MirrorVerticallyCommand : ExCommand!() {
    this() { super("Mirror Vertically"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                mirrorAll(param, 1);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class MirroredAutoFillDir1Command : ExCommand!() {
    this() { super("Mirrored Autofill "); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                mirroredAutofill(param, 0, 0, 0.4999);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class MirroredAutoFillDir2Command : ExCommand!() {
    this() { super("Mirrored Autofill "); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                mirroredAutofill(param, 0, 0.5001, 1);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class MirroredAutoFillDir3Command : ExCommand!() {
    this() { super("Mirrored Autofill "); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                mirroredAutofill(param, 1, 0.5001, 1);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class MirroredAutoFillDir4Command : ExCommand!() {
    this() { super("Mirrored Autofill "); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            foreach (param; ctx.parameters) {
                mirroredAutofill(param, 1, 0, 0.4999);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class CopyParameterCommand : ExCommand!() {
    this() { super("Copy Parameter"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];
                nijigenerate.commands.parameter.base.cClipboardParameter = param.dup;
            }
        }
    }
}

class PasteParameterCommand : ExCommand!() {
    this() { super("Paste Parameter"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];
                pasteParameter(param, null, 2);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class PasteParameterWithFlipCommand : ExCommand!() {
    this() { super("Paste Parameter with Flip"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];
                pasteParameter(param, null, 0);
                incViewportNodeDeformNotifyParamValueChanged();
            }
        }
    }
}

class DuplicateParameterCommand : ExCommand!() {
    this() { super("Duplicate Parameter"); }
    override
    void run(Context ctx) {
        if (ctx.hasPuppet && ctx.hasParameters) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];

                Parameter newParam = param.dup;
                ctx.puppet.parameters ~= newParam;
                newParam.bindings.length = 0;
                pasteParameter!false(newParam, param, 0);
                if (auto exParam = cast(ExParameter)newParam) {
                    exParam.setParent((cast(ExParameter)param).getParent());
                }
                incActionPush(new ParameterAddAction(newParam, cast(Parameter[]*)[])); //parentList is not used. so passed [].
            }
        }
    }
}

class DuplicateParameterWithFlipCommand : ExCommand!() {
    this() { super("Duplicate Parameter with Flip"); }
    override
    void run(Context ctx) {
        if (ctx.hasPuppet && ctx.hasParameters) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];

                Parameter newParam = param.dup;
                ctx.puppet.parameters ~= newParam;
                if (auto exParam = cast(ExParameter)newParam) {
                    exParam.setParent((cast(ExParameter)param).getParent());
                }
                incActionPush(new ParameterAddAction(newParam));
            }
        }
    }
}

class DeleteParameterCommand : ExCommand!() {
    this() { super("Delete Parameter"); }
    override
    void run(Context ctx) {
        if (ctx.hasPuppet && ctx.hasParameters) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];

                if (ctx.puppet == param) {
                    incDisarmParameter();
                }
                incActionPush(new ParameterRemoveAction(param));
                ctx.puppet.removeParameter(param);
            }
        }
    }
}

void addBinding(Parameter param, Parameter p, int fromAxis, int toAxis) {
    auto binding = param.createBinding(p, toAxis);
    param.addBinding(binding);
//                    writefln("%s", param.bindings);
    auto ppBinding = cast(ParameterParameterBinding)binding;
    vec2 pos;
    vec2u index;

    pos = vec2(0, 0);
    pos.vector[fromAxis] = param.min.vector[fromAxis];
    index = param.findClosestKeypoint(pos);
    ppBinding.values[index.vector[0]][index.vector[1]] = p.min.vector[toAxis];
    ppBinding.isSet_[index.vector[0]][index.vector[1]] = true;

    pos = vec2(0, 0);
    pos.vector[fromAxis] = param.max.vector[fromAxis];
    index = param.findClosestKeypoint(pos);
    ppBinding.values[index.vector[0]][index.vector[1]] = p.max.vector[toAxis];
    ppBinding.isSet_[index.vector[0]][index.vector[1]] = true;

    ppBinding.reInterpolate();
    auto action = new ParameterBindingAddAction(param, binding);
    incActionPush(action);
}

class LinkToCommand : ExCommand!(Parameter, int, int) {
    this(Parameter p, int fromAxis, int toAxis) { super("Delete Parameter", p, fromAxis, toAxis); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];
                addBinding(param, arg0, arg1, arg2);
            }
        }
    }
}


class ToggleParameterArmCommand : ExCommand!(int) {
    this(int index) { super("Toggle Armed Parameter", index); }
    override
    void run(Context ctx) {
        if (ctx.hasPuppet && ctx.hasParameters) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];
 
                if (ctx.puppet == param) {
                    incDisarmParameter();
                } else {
                    param.value = param.getClosestKeypointValue();
                    paramPointChanged(param);
                    incArmParameter(arg0, param);
                }
            }
        }
    }
}

class SetStartingKeyFrameCommand : ExCommand!() {
    this() { super("Set Starting KeyFrame"); }
    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];

                auto action = new ParameterValueChangeAction!vec2("axis points", param, &param.defaults);
                param.defaults = param.value;
                action.updateNewState();
                incActionPush(action); 
            }
        }
    }
}

enum ParameditCommand {
    ConvertTo2DParam,
    FlipX,
    FlipY,
    Flip1D,
    MirrorHorizontally,
    MirrorVertically,
    MirroredAutoFillDir1,
    MirroredAutoFillDir2,
    MirroredAutoFillDir3,
    MirroredAutoFillDir4,
    CopyParameter,
    PasteParameter,
    PasteParameterWithFlip,
    DuplicateParameter,
    DuplicateParameterWithFlip,
    DeleteParameter,
    LinkTo,
    ToggleParameterArm,
    SetStartingKeyFrame
}


// コマンド登録用 mixin 定義（文字列引数方式）
template register(alias id, string args = "") {
    import std.string : format;
    enum name = __traits(identifier, id);
    enum parentName = __traits(identifier, __traits(parent, id));
    enum ctor = name ~ "Command";
    static if (args.length == 0)
        enum register = format("commands[%s.%s] = new %s();", parentName, name, ctor);
    else
        enum register = format("commands[%s.%s] = new %s(%s);", parentName, name, ctor, args);
}

private {
    Command[ParameditCommand] commands;

    static this() {
        import std.traits : EnumMembers;
        // 自動登録：コンパイル可能なコマンドのみ mixin
        static foreach (name; EnumMembers!ParameditCommand) {
            static if (__traits(compiles, mixin(register!(name)))) {
                mixin(register!(name));
            }
        }
        // 引数付きコマンドはマニアル登録
        mixin(register!(ParameditCommand.LinkTo, "null, 0, 0"));
        mixin(register!(ParameditCommand.ToggleParameterArm, "0"));
    }
}
