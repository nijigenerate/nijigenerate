module nijigenerate.commands.binding.base;

import nijigenerate.commands.base;

import nijigenerate.commands.parameter.base;
import nijigenerate.viewport.model.deform;
import nijigenerate.panels;
import nijigenerate.ext.param;
//import nijigenerate.widgets;
import nijigenerate.windows;
import nijigenerate.core.math.triangle;
import nijigenerate.core;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.ext.param;
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


ParameterBinding[][nijilive.core.Resource] cParamBindingEntries;
ParameterBinding[][nijilive.core.Resource] cParamBindingEntriesAll;
//nijilive.core.Resource[] cAllBoundNodes;
ParameterBinding[BindTarget] cSelectedBindings;
//    ParameterBinding[BindTargetBase!(Parameter, int)] cSelectedParameterParameterBindings;
nijilive.core.Resource[] cCompatibleNodes;
vec2u cParamPoint;
vec2u cClipboardPoint;
ParameterBinding[BindTarget] cClipboardBindings;
Parameter cClipboardParameter = null;
bool selectedOnly = false;

void refreshBindingList(Parameter param, bool selectedOnly = false) {
    // Filter selection to remove anything that went away
    ParameterBinding[BindTarget] newSelectedBindings;

    auto selected = selectedOnly? incSelectedNodes() : [];
    cParamBindingEntriesAll.clear();
    foreach(ParameterBinding binding; param.bindings) {
        auto target = binding.getTarget();
        if (target in cSelectedBindings) newSelectedBindings[target] = binding;
        if (!selectedOnly || selected.countUntil(target.node) >= 0)
            cParamBindingEntriesAll[binding.getNode()] ~= binding;
    }
    cAllBoundNodes = cParamBindingEntriesAll.keys.dup;
    bool compare(nijilive.core.Resource x, nijilive.core.Resource y) {
        Node nx, ny;
        nx = cast(Node)x;
        ny = cast(Node)y;
        if (nx && ny) {
            return nx.name < ny.name;
        }
        if (nx) return true;
        if (ny) return false;
        Parameter px, py;
        px = cast(Parameter)x;
        py = cast(Parameter)y;
        if (px && py) {
            return px.name < py.name;
        }
        return false;
    }
    cAllBoundNodes.sort!(compare);
    cSelectedBindings = newSelectedBindings;
    paramPointChanged(param);
}

void paramPointChanged(Parameter param) {
    cParamBindingEntries.clear();

    cParamPoint = param.findClosestKeypoint();
    foreach(ParameterBinding binding; param.bindings) {
        if (binding.isSet(cParamPoint)) {
            cParamBindingEntries[binding.getTarget.target] ~= binding;
        }
    }
}

nijilive.core.Resource[] getCompatibleNodes() {
    Node thisNode = null;

    foreach(binding; cSelectedBindings.byValue()) {
        if (auto node = cast(Node)binding.getTarget.target) {
            if (thisNode is null) thisNode = node;
            else if (node !is thisNode) return null;
        }
    }
    if (thisNode is null) return null;

    nijilive.core.Resource[] compatible;
    nodeLoop: foreach(another; cParamBindingEntriesAll.byKey()) {
        Node otherNode = cast(Node)(another);
        if (otherNode is thisNode) continue;

        foreach(binding; cSelectedBindings.byValue()) {
            if (!binding.isCompatibleWithNode(otherNode)) {
                continue nodeLoop;
            } else {
                continue nodeLoop;
            }
        }
        compatible ~= cast(nijilive.core.Resource)otherNode;
    }

    return compatible;
}

void copySelectionToNode(Parameter param, Node target) {
    nijilive.core.Resource src = cSelectedBindings.keys[0].target;

    foreach(binding; cSelectedBindings.byValue()) {
        if (auto node = cast(Node)binding.getTarget.target) {
            assert(binding.getTarget.target is src, "selection mismatch");

            ParameterBinding b = param.getOrAddBinding(target, binding.getTarget.name);
            binding.copyKeypointToBinding(cParamPoint, b, cParamPoint);
        }
    }
    target.notifyChange(target, NotifyReason.StructureChanged);

    refreshBindingList(param);
}

void swapSelectionWithNode(Parameter param, Node target) {
    nijilive.core.Resource src = cSelectedBindings.keys[0].target;

    foreach(binding; cSelectedBindings.byValue()) {
        if (auto node = cast(Node)binding.getTarget.target) {
            assert(binding.getTarget().target is src, "selection mismatch");

            ParameterBinding b = param.getOrAddBinding(target, binding.getTarget().name);
            binding.swapKeypointWithBinding(cParamPoint, b, cParamPoint);
        }
    }

    refreshBindingList(param);
}


struct ParamDragDropData {
    Parameter param;
}
