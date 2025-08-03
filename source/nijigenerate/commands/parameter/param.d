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



void incParameterMenuContents(Parameter[] parameters) {
    if (igMenuItem(__("Add 1D Parameter (0..1)"), "", false, true)) {
        Parameter param = new ExParameter(
            "Param #%d\0".format(parameters.length),
            false
        );
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
    }
    if (igMenuItem(__("Add 1D Parameter (-1..1)"), "", false, true)) {
        Parameter param = new ExParameter(
            "Param #%d\0".format(parameters.length),
            false
        );
        param.min.x = -1;
        param.max.x = 1;
        param.insertAxisPoint(0, 0.5);
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
    }
    if (igMenuItem(__("Add 2D Parameter (0..1)"), "", false, true)) {
        Parameter param = new ExParameter(
            "Param #%d\0".format(parameters.length),
            true
        );
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
    }
    if (igMenuItem(__("Add 2D Parameter (-1..+1)"), "", false, true)) {
        Parameter param = new ExParameter(
            "Param #%d\0".format(parameters.length),
            true
        );
        param.min = vec2(-1, -1);
        param.max = vec2(1, 1);
        param.insertAxisPoint(0, 0.5);
        param.insertAxisPoint(1, 0.5);
        incActivePuppet().parameters ~= param;
        incActionPush(new ParameterAddAction(param, &incActivePuppet().parameters));
    }
    if (igMenuItem(__("Add Mouth Shape"), "", false, true)) {
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

