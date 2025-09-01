module nijigenerate.commands.parameter.base;
import nijigenerate.viewport.model.deform;
import nijigenerate.panels;
import nijigenerate.ext.param;
//import nijigenerate.widgets;
//import nijigenerate.windows;
import nijigenerate.core.math.triangle;
import nijigenerate.core;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.common.mesh;
import nijigenerate.windows.flipconfig;
import nijigenerate.viewport.model.onionslice;
import nijigenerate.utils.transform;
import nijigenerate;
import std.string;
import nijilive;
import i18n;
import std.uni : toLower;
//import std.stdio;
import nijigenerate.utils;
import std.algorithm.searching : countUntil;
import std.algorithm.sorting : sort;
import std.algorithm.mutation : remove;
import nijigenerate.commands.binding.base;

nijilive.core.Resource[] cAllBoundNodes;

void mirrorAll(Parameter param, uint axis) {
    auto action = new ParameterChangeBindingsAction("Mirror All", param, null);
    foreach(ParameterBinding binding; param.bindings) {
        uint xCount = param.axisPointCount(0);
        uint yCount = param.axisPointCount(1);
        foreach(x; 0..xCount) {
            foreach(y; 0..yCount) {
                vec2u index = vec2u(x, y);
                if (binding.isSet(index)) {
                    binding.scaleValueAt(index, axis, -1);
                }
            }
        }
    }
    action.updateNewState();
    incActionPush(action);
}

void mirroredAutofill(Parameter param, uint axis, float min, float max) {
    incActionPushGroup();
    auto action = new ParameterChangeBindingsAction("Mirror Auto Fill", param, null);

    foreach(ParameterBinding binding; param.bindings) {
        if (auto target = cast(Node)binding.getTarget().target) {
            auto pair = incGetFlipPairFor(target);
            auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget.name, true);
            // Check if the binding was found or created
            if(targetBinding is null) continue;

            uint xCount = param.axisPointCount(0);
            uint yCount = param.axisPointCount(1);
            foreach(x; 0..xCount) {
                float offX = param.axisPoints[0][x];
                if (axis == 0 && (offX < min || offX > max)) continue;
                foreach(y; 0..yCount) {
                    float offY = param.axisPoints[1][y];
                    if (axis == 1 && (offY < min || offY > max)) continue;

                    vec2u index = vec2u(x, y);
                    if (!targetBinding.isSet(index)) incBindingAutoFlip(targetBinding, binding, index, axis);
                }
            }
        }
    }
    action.updateNewState();
    incActionPush(action);
    incActionPopGroup();
}

void pasteParameter(bool pushAction = true)(Parameter param, Parameter srcParam = null, uint axis = 2) {
    if (srcParam is null)
        srcParam = cClipboardParameter;
    if (srcParam is null)
        return;
    ParameterChangeBindingsAction action = null;
    static if (pushAction) {
        incActionPushGroup();
        action = new ParameterChangeBindingsAction("Paste", param, null);
    }

    foreach(ParameterBinding srcBinding; srcParam.bindings) {
        if (auto target = cast(Node)srcBinding.getTarget().target) {
            FlipPair pair = null;
            if (axis != 2)
                pair = incGetFlipPairFor(target);
            auto binding = incBindingGetPairFor(param, target, pair, srcBinding.getTarget.name, true);
            // Check if the binding was found or created
            if(binding is null) continue;

            uint xCount = param.axisPointCount(0);
            uint yCount = param.axisPointCount(1);
            foreach(x; 0..xCount) {
                foreach(y; 0..yCount) {
                    vec2u index = vec2u(x, y);
                    incBindingAutoFlip(binding, srcBinding, index, axis, false);
                }
            }
        } else {
            //FIXME: must be implemented.                
        }
    }
    static if (pushAction) {
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
    }
    if (srcParam == cClipboardParameter)
        cClipboardParameter = null;
}


void convertTo2D(Parameter param) {
    auto action = new GroupAction();

    auto newParam = new ExParameter(param.name, true);
    newParam.uuid = param.uuid;
    newParam.min  = vec2(param.min.x, param.min.x);
    newParam.max  = vec2(param.max.x, param.max.x);
    long findIndex(T)(T[] array, T target) {
        ptrdiff_t idx = array.countUntil(target);
        return idx;
    }
    foreach (key; param.axisPoints[0]) {
        if (key != 0 && key != 1) {
            newParam.insertAxisPoint(0, key);
        }
        foreach(binding; param.bindings) {
            ParameterBinding b = newParam.getOrAddBinding(binding.getTarget().target, binding.getTarget().name);
            auto srcKeyIndex  = param.findClosestKeypoint(param.unmapValue(vec2(key, 0)));
            auto destKeyIndex = newParam.findClosestKeypoint(newParam.unmapValue(vec2(key, newParam.min.y)));
            binding.copyKeypointToBinding(srcKeyIndex, b, destKeyIndex);
        }
    }
    auto index = incActivePuppet().parameters.countUntil(param);
    if (index >= 0) {
        action.addAction(new ParameterRemoveAction(param, &incActivePuppet().parameters));
        action.addAction(new ParameterAddAction(newParam, &incActivePuppet().parameters));
        incActivePuppet().parameters[index] = newParam;
        if (auto prevParam = cast(ExParameter)param) {
            auto parent = prevParam.getParent();
            prevParam.setParent(null);
            newParam.setParent(parent);
        }
    }
    incActionPush(action);
}

bool removeParameter(Parameter param) {
    ExParameterGroup parent = null;
    ptrdiff_t idx = -1;

    mloop: foreach(i, iparam; incActivePuppet.parameters) {
        if (iparam.uuid == param.uuid) {
            idx = i;
            break mloop;
        }

        if (ExParameterGroup group = cast(ExParameterGroup)iparam) {
            foreach(x, ref xparam; group.children) {
                if (xparam.uuid == param.uuid) {
                    idx = x;
                    parent = group;
                    break mloop;
                }
            }
        }
    }

    if (idx < 0) return false;

    if (parent) {
        if (parent.children.length > 0) parent.children = parent.children.remove(idx);
        else parent.children.length = 0;
    }
    if (incActivePuppet().parameters.length > 1) incActivePuppet().parameters = incActivePuppet().parameters.remove(idx);
    else incActivePuppet().parameters.length = 0;

    return true;
}



void incMoveParameter(Parameter from, ExParameterGroup to = null, int index = 0) {
    (cast(ExParameter)from).setParent(to);
}

ExParameterGroup incCreateParamGroup(int index = 0) {
    import std.array : insertInPlace;

    if (index < 0) index = 0;
    else if (index > incActivePuppet().parameters.length) index = cast(int)incActivePuppet().parameters.length-1;

    auto group = new ExParameterGroup(_("New Parameter Group"));
    (cast(ExPuppet)incActivePuppet()).addGroup(group);
    return group;
}
