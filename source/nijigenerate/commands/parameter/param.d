module nijigenerate.commands.parameter.param;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijilive;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.windows;
import nijigenerate.core;
import nijigenerate.project;
import nijigenerate.actions;
import i18n;

class Add1DParameterCommand(int min, int max) : ExCommand!() {
    this() { super("Add 1D Parameter (%d..%d)".format(min, max)); }
    override
    void run(Context ctx) {
        if (!ctx.hasPuppet)
            return;
        
        Parameter param = new ExParameter(
            "Param #%d\0".format(parameters.length),
            false
        );
        param.min.x = min;
        param.max.x = max;
        if (min + max == 0)
            param.insertAxisPoint(0, 0.5);
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
    }
}

class Add2DParameterCommand(int min, int max) : ExCommand!() {
    this() { super("Add 2D Parameter (%d..%d)".format(min, max)); }
    override
    void run(Context ctx) {
        if (!ctx.hasPuppet)
            return;
        
        Parameter param = new ExParameter(
            "Param #%d\0".format(parameters.length),
            true
        );
        param.min = vec2(min, min);
        param.max = vec2(max, max);
        if (min + max == 0) {
            param.insertAxisPoint(0, 0.5);
            param.insertAxisPoint(1, 0.5);
        }
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
    }
}

class AddMouthParameterCommand(int min, int max) : ExCommand!() {
    this() { super("Add Mouth Parameter (%d..%d)".format(min, max)); }
    override
    void run(Context ctx) {
        if (!ctx.hasPuppet)
            return;
        
        Parameter param = new ExParameter(
            "Mouth #%d\0".format(parameters.length),
            true
        );
        param.min = vec2(-1, 0);
        param.max = vec2(1, 1);
        param.insertAxisPoint(0, 0.25);
        param.insertAxisPoint(0, 0.5);
        param.insertAxisPoint(0, 0.6);
        param.insertAxisPoint(1, 0.3);
        param.insertAxisPoint(1, 0.5);
        param.insertAxisPoint(1, 0.6);
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
    }
}