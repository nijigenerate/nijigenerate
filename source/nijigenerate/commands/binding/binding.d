module nijigenerate.commands.binding.binding;

import nijigenerate.commands.base;

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
import std.array;
import nijilive;
import i18n;
import std.uni : toLower;
//import std.stdio;
import nijigenerate.utils;
import std.algorithm.searching : countUntil;
import std.algorithm.sorting : sort;
import std.algorithm.mutation : remove;

import nijigenerate.commands.binding.base;

//==================================================================================
// Command Palette Definition for Key Frame
//==================================================================================

class UnsetKeyFrameCommand : ExCommand!() {
    this() { super("Unset Key Frame"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        auto action = new ParameterChangeBindingsValueAction("unset", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.unset(cParamPoint);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class SetKeyFrameCommand : ExCommand!() {
    this() { super("Set Key Frame"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        auto action = new ParameterChangeBindingsValueAction("setCurrent", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.setCurrent(cParamPoint);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class ResetKeyFrameCommand : ExCommand!() {
    this() { super("Reset Key Frame"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("reset", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.reset(cParamPoint);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class InvertKeyFrameCommand : ExCommand!() {
    this() { super("Invert Key Frame"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("invert", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.scaleValueAt(cParamPoint, -1, -1);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class MirrorKeyFrameHorizontallyCommand : ExCommand!() {
    this() { super("Mirror Key Frame Horizontally"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("mirror Horizontally", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.scaleValueAt(cParamPoint, 0, -1);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class MirrorKeyFrameVerticallyCommand : ExCommand!() {
    this() { super("Mirror Key Frame Vertically"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("mirror Vertically", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.scaleValueAt(cParamPoint, 1, -1);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class FlipDeformCommand : ExCommand!() {
    this() { super("Flip Deform"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("Flip Deform", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            auto deformBinding = cast(DeformationParameterBinding)binding;  
            if (deformBinding is null) continue;
            auto newDeform = deformByDeformationBinding(deformBinding, deformBinding, vec2u(cParamPoint.x, cParamPoint.y), true);
            if (newDeform)
                deformBinding.setValue(cParamPoint, *newDeform);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class SymmetrizeDeformCommand : ExCommand!() {
    this() { super("Symmetrize Deform"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("Symmetrize Deform", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            auto deformBinding = cast(DeformationParameterBinding)binding;  
            if (deformBinding is null) continue;
            auto newDeform = deformByDeformationBinding(deformBinding, deformBinding, vec2u(cParamPoint.x, cParamPoint.y), true);
            if (newDeform && newDeform.vertexOffsets.length == deformBinding.values[cParamPoint.x][cParamPoint.y].vertexOffsets.length) {
                foreach(i; 0..(*newDeform).vertexOffsets.length) {
                    (*newDeform).vertexOffsets[i] = (deformBinding.values[cParamPoint.x][cParamPoint.y].vertexOffsets[i] + (*newDeform).vertexOffsets[i]) / 2;
                }
                deformBinding.setValue(cParamPoint, *newDeform);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class SetFromHorizontalMirrorCommand : ExCommand!(TW!(bool, "targetBindingsNull", "specify whether targetBindings is null or not.")) {
    this(bool targetBindingsNull) { super("Set From Horizontal Mirror", targetBindingsNull); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror (Horizontally)", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget().name, targetBindingsNull);
                if (targetBindingsNull)
                    incBindingAutoFlip(binding, targetBinding, cParamPoint, 0);
                else if(targetBinding !is null)
                    incBindingAutoFlip(targetBinding, binding, cParamPoint, 0);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class SetFromVerticalMirrorCommand : ExCommand!(TW!(bool, "targetBindingsNull", "specify whether targetBindings is null or not.")) {
    this(bool targetBindingsNull) { super("Set From Vertical Mirror", targetBindingsNull); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror (Vertically)", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget().name, targetBindingsNull);
                if (targetBindingsNull)
                    incBindingAutoFlip(binding, targetBinding, cParamPoint, 1);
                else if(targetBinding !is null)
                    incBindingAutoFlip(targetBinding, binding, cParamPoint, 1);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();
    }
}


class SetFromDiagonalMirrorCommand : ExCommand!(TW!(bool, "targetBindingsNull", "specify whether targetBindings is null or not.")) {
    this(bool targetBindingsNull) { super("Set From Diagonal Mirror", targetBindingsNull); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror (Diagonally)", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget().name, targetBindingsNull);
                if (targetBindingsNull)
                    incBindingAutoFlip(binding, targetBinding, cParamPoint, -1);
                else if(targetBinding !is null)
                    incBindingAutoFlip(targetBinding, binding, cParamPoint, -1);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();
    }
}


class SetFrom1DMirrorCommand : ExCommand!(TW!(bool, "targetBindingsNull", "specify whether targetBindings is null or not.")) {
    this(bool targetBindingsNull) { super("Set From 1D Mirror", targetBindingsNull); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget.name, targetBindingsNull);
                if (targetBindingsNull)
                    incBindingAutoFlip(binding, targetBinding, cParamPoint, 0);
                else if(targetBinding !is null)
                    incBindingAutoFlip(targetBinding, binding, cParamPoint, 0);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();        
    }
}

class CopyBindingCommand : ExCommand!() {
    this() { super("Copy Bindings"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        cClipboardPoint = cParamPoint;
        cClipboardBindings.clear();
        foreach(binding; bindings) {
            cClipboardBindings[binding.getTarget()] = binding;
        }

    }
}

class PasteBindingCommand : ExCommand!() {
    this() { super("Paste Bindings"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings || !ctx.hasKeyPoint)
            return;
        
        auto param = ctx.parameters[0];
        auto bindings = ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        // Find the bindings we should apply
        // This allows us to skip the application process if we can't apply anything.
        ParameterBinding[] bindingsToApply;
        foreach(ref binding; bindings) {
            if (binding.getTarget() in cClipboardBindings) bindingsToApply ~= binding;
        }

        // Whether there's only a single binding, if so, we should not push a group
        bool isSingle = (bindings.length == 1 && cClipboardBindings.length == 1) || bindingsToApply.length == 1;

        if (bindingsToApply.length > 0) {
            if (!isSingle) incActionPushGroup();
            foreach(binding; bindingsToApply) {
                auto action = new ParameterChangeBindingsValueAction("paste", param, bindings, cParamPoint.x, cParamPoint.y);
                ParameterBinding origBinding = cClipboardBindings[binding.getTarget()];
                origBinding.copyKeypointToBinding(cClipboardPoint, binding, cParamPoint);
                action.updateNewState();
                incActionPush(action);
            }
            if (!isSingle) incActionPopGroup();
        } else if (bindings.length == 1 && cClipboardBindings.length == 1) {
            ParameterBinding binding = bindings[0];
            ParameterBinding srcBinding = cClipboardBindings.values[0];
            if (is(typeof(binding) == typeof(srcBinding))) {
                auto action = new ParameterChangeBindingsValueAction("paste", param, bindings, cParamPoint.x, cParamPoint.y);
                if (auto deformBinding = cast(DeformationParameterBinding)(binding)) {
                    auto newDeform = deformByDeformationBinding(deformBinding, cast(DeformationParameterBinding)srcBinding, cParamPoint, false);
                    if (newDeform)
                        deformBinding.setValue(cParamPoint, *newDeform);

                } else {
                    ValueParameterBinding valueBinding = cast(ValueParameterBinding)(binding);
                    ValueParameterBinding valueSrcBinding = cast(ValueParameterBinding)(srcBinding);
                    valueBinding.setValue(cParamPoint, valueSrcBinding.getValue(cClipboardPoint));
                }
                action.updateNewState();
                incActionPush(action);
            }
        }

    }
}

//==================================================================================
// Command Palette Definition for Binding
//==================================================================================

class RemoveBindingCommand : ExCommand!() {
    this() { super("Remove Bindings"); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings)
            return;
        
        auto param = ctx.parameters[0];

        auto action = new GroupAction();
        foreach(binding; cSelectedBindings.byValue()) {
            action.addAction(new ParameterBindingRemoveAction(param, binding));
            param.removeBinding(binding);
            if (auto node = cast(Node)binding.getTarget().target)
                node.notifyChange(node, NotifyReason.StructureChanged);
        }
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

class SetInterpolationCommand : ExCommand!(TW!(InterpolateMode, "mode", "specify the new interpolation mode.")) {
    this(InterpolateMode mode) { super("Set Bindings to " ~ mode.stringof, mode); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || !ctx.hasBindings)
            return;
        
        auto param = ctx.parameters[0];
        foreach(binding; cSelectedBindings.values) {
            binding.interpolateMode = mode;
        }
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

/*
CopyTo
        foreach(c; cCompatibleNodes) {
            if (Node cNode = cast(Node)c) {
                if (igMenuItem(cNode.name.toStringz, "", false, true)) {
                    copySelectionToNode(param, cNode);
                }
            }
        }
*/

/*
Swap with
        foreach(c; cCompatibleNodes) {
            if (Node cNode = cast(Node)c) {
                if (igMenuItem(cNode.name.toStringz, "", false, true)) {
                    swapSelectionWithNode(param, cNode);
                }
            }
        }
*/


enum BindingCommand {
    UnsetKeyFrame,
    SetKeyFrame,
    ResetKeyFrame,
    InvertKeyFrame,
    MirrorKeyFrameHorizontally,
    MirrorKeyFrameVertically,
    FlipDeform,
    SymmetrizeDeform,
    SetFromHorizontalMirror,
    SetFromVerticalMirror,
    SetFromDiagonalMirror,
    SetFrom1DMirror,
    CopyBinding,
    PasteBinding,
    RemoveBinding,
    SetInterpolation
}


import nijigenerate.commands.base : registerCommand;

Command[BindingCommand] commands;
private {
    static this() {
        import std.traits : EnumMembers;

        static foreach (name; EnumMembers!BindingCommand) {
            static if (__traits(compiles, { mixin(registerCommand!(name)); } ))
                mixin(registerCommand!(name));
        }

        mixin(registerCommand!(BindingCommand.SetFromHorizontalMirror, false));
        mixin(registerCommand!(BindingCommand.SetFromVerticalMirror, false));
        mixin(registerCommand!(BindingCommand.SetFromDiagonalMirror, false));
        mixin(registerCommand!(BindingCommand.SetFrom1DMirror, false));
        mixin(registerCommand!(BindingCommand.SetInterpolation, InterpolateMode.Linear));
    }
}
