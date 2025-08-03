module nijigenerate.commands.parameter.animedit;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijigenerate.project;

class AddKeyFrameCommand : ExCommand!() {
    this() { super("Add KeyFrame"); }

    override
    void run(Context ctx) {
        if (ctx.hasParameters()) {
            if (ctx.parameters.length != 0) {
                auto param = ctx.parameters[0];

                if (param.isVec2) {
                    incAnimationKeyframeAdd(param, 0, param.value.vector[0]);
                    incAnimationKeyframeAdd(param, 1, param.value.vector[1]);
                } else {
                    incAnimationKeyframeAdd(param, 0, param.value.vector[0]);
                }
            }
        }
    }
}