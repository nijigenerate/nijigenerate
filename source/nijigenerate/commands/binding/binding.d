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
    this() { super(_("Unset Key Frame")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        auto action = new ParameterChangeBindingsValueAction("unset", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.unset(cParamPoint);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}

class SetKeyFrameCommand : ExCommand!() {
    this() { super(_("Set Key Frame")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        auto action = new ParameterChangeBindingsValueAction("setCurrent", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.setCurrent(cParamPoint);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}

class ResetKeyFrameCommand : ExCommand!() {
    this() { super(_("Reset Key Frame")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("reset", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.reset(cParamPoint);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}

class InvertKeyFrameCommand : ExCommand!() {
    this() { super(_("Invert Key Frame")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("invert", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.scaleValueAt(cParamPoint, -1, -1);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}

class MirrorKeyFrameHorizontallyCommand : ExCommand!() {
    this() { super(_("Mirror Key Frame Horizontally")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("mirror Horizontally", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.scaleValueAt(cParamPoint, 0, -1);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}

class MirrorKeyFrameVerticallyCommand : ExCommand!() {
    this() { super(_("Mirror Key Frame Vertically")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;
        
        auto action = new ParameterChangeBindingsValueAction("mirror Vertically", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            binding.scaleValueAt(cParamPoint, 1, -1);
        }
        action.updateNewState();
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}

class FlipDeformCommand : ExCommand!() {
    this() { super(_("Flip Deform")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
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
        return CommandResult(true);
    }
}

class SymmetrizeDeformCommand : ExCommand!() {
    this() { super(_("Symmetrize Deform")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.armedParameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
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
        return CommandResult(true);
    }
}

class SetFromHorizontalMirrorCommand : ExCommand!() {
    this() { super(_("Set From Horizontal Mirror")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto param = ctx.armedParameters[0];
        auto cParamPoint = ctx.keyPoint;

        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror (Horizontally)", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget().name, true);
                incBindingAutoFlip(binding, targetBinding, cParamPoint, 0);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}

class SetFromVerticalMirrorCommand : ExCommand!() {
    this() { super(_("Set From Vertical Mirror")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto param = ctx.parameters[0];
        auto cParamPoint = ctx.keyPoint;
        
        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror (Vertically)", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget().name, true);
                incBindingAutoFlip(binding, targetBinding, cParamPoint, 1);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}


class SetFromDiagonalMirrorCommand : ExCommand!() {
    this() { super(_("Set From Diagonal Mirror")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto param = ctx.parameters[0];
        auto cParamPoint = ctx.keyPoint;

        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror (Diagonally)", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget().name, true);
                incBindingAutoFlip(binding, targetBinding, cParamPoint, -1);
            }
        }
        action.updateNewState();
        incActionPush(action);
        incActionPopGroup();
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
    }
}


class SetFrom1DMirrorCommand : ExCommand!() {
    this() { super(_("Set From 1D Mirror")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto param = ctx.parameters[0];
        auto cParamPoint = ctx.keyPoint;

        incActionPushGroup();
        auto action = new ParameterChangeBindingsValueAction("set From Mirror", param, bindings, cParamPoint.x, cParamPoint.y);
        foreach(binding; bindings) {
            if (auto target = cast(Node)binding.getTarget().target) {
                auto pair = incGetFlipPairFor(target);
                auto targetBinding = incBindingGetPairFor(param, target, pair, binding.getTarget.name, true);
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
        return CommandResult(true);
    }
}

class CopyBindingCommand : ExCommand!() {
    this() { super(_("Copy Bindings")); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return CommandResult(false, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.parameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        cClipboardPoint = cParamPoint;
        cClipboardBindings.clear();
        foreach(binding; bindings) {
            cClipboardBindings[binding.getTarget()] = binding;
        }
        return CommandResult(true);
    }
}

class PasteBindingCommand : ExCommand!() {
    this() { super(_("Paste Bindings")); }
    override
    CreateResult!ParameterBinding run(Context ctx) {
        if (!ctx.hasArmedParameters || ctx.armedParameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings) || !ctx.hasKeyPoint)
            return new CreateResult!ParameterBinding(false, null, "No armed parameter/bindings/keypoint");
        
        auto param = ctx.parameters[0];
        bool targetBindingsNull = !ctx.hasActiveBindings || ctx.activeBindings is null;
        auto bindings = (!targetBindingsNull)? ctx.activeBindings: ctx.bindings;
        auto cParamPoint = ctx.keyPoint;

        // Build list of bindings to apply and create missing ones when appropriate
        bool explicitTargets = !targetBindingsNull;
        ParameterBinding[] targetsToApply;
        ParameterBinding[] sourceBindings;
        ParameterBinding[] newlyCreatedBindings;

        if (explicitTargets) {
            foreach (binding; bindings) {
                auto targetKey = binding.getTarget();
                ParameterBinding* srcBinding = targetKey in cClipboardBindings;
                if (!srcBinding)
                    continue;

                auto ensured = param.getOrAddBinding(targetKey.target, targetKey.name);
                if (ensured !is binding)
                    newlyCreatedBindings ~= ensured;

                targetsToApply ~= ensured;
                sourceBindings ~= *srcBinding;
            }
        } else {
            foreach (kv; cClipboardBindings.byKeyValue) {
                auto targetKey = kv.key;
                auto srcBinding = kv.value;
                auto destBinding = param.getBinding(targetKey.target, targetKey.name);
                if (destBinding is null) {
                    destBinding = param.getOrAddBinding(targetKey.target, targetKey.name);
                    newlyCreatedBindings ~= destBinding;
                }
                targetsToApply ~= destBinding;
                sourceBindings ~= srcBinding;
            }
        }

        CreateResult!ParameterBinding resPayload;
        bool payloadSet = false;

        if (targetsToApply.length > 0) {
            bool isSingle = targetsToApply.length == 1 && cClipboardBindings.length == 1;
            if (!isSingle) incActionPushGroup();
            if (newlyCreatedBindings.length > 0) {
                auto addAction = new ParameterAddBindingsAction("paste", param, newlyCreatedBindings);
                addAction.updateNewState();
                incActionPush(addAction);
                resPayload = new CreateResult!ParameterBinding(true, newlyCreatedBindings);
                payloadSet = true;
            }
            foreach (i, targetBinding; targetsToApply) {
                auto action = new ParameterChangeBindingsValueAction("paste", param, [targetBinding], cParamPoint.x, cParamPoint.y);
                auto srcBinding = sourceBindings[i];
                srcBinding.copyKeypointToBinding(cClipboardPoint, targetBinding, cParamPoint);
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

        if (payloadSet) return resPayload;
        return new CreateResult!ParameterBinding(true, null, "");

    }
}

//==================================================================================
// Command Palette Definition for Binding
//==================================================================================

class RemoveBindingCommand : ExCommand!() {
    this() { super(null, _("Remove Bindings")); }
    override
    DeleteResult!ParameterBinding run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings))
            return new DeleteResult!ParameterBinding(false, null, "No parameters/bindings");
        
        auto param = ctx.parameters[0];

        auto action = new GroupAction();
        ParameterBinding[] removed;
        foreach(binding; cSelectedBindings.byValue()) {
            action.addAction(new ParameterBindingRemoveAction(param, binding));
            param.removeBinding(binding);
            if (auto node = cast(Node)binding.getTarget().target)
                node.notifyChange(node, NotifyReason.StructureChanged);
            removed ~= binding;
        }
        incActionPush(action);
        incViewportNodeDeformNotifyParamValueChanged();
        if (removed.length) {
            return new DeleteResult!ParameterBinding(true, removed, "Bindings removed");
        }
        return new DeleteResult!ParameterBinding(false, null, "No bindings removed");
    }
}

class SetInterpolationCommand : ExCommand!(TW!(InterpolateMode, "mode", "specify the new interpolation mode.")) {
    this(InterpolateMode mode) { super(null, "Set Bindings to " ~ mode.stringof, mode); }
    override
    CommandResult run(Context ctx) {
        if (!ctx.hasParameters || ctx.parameters.length == 0 || (!ctx.hasBindings && !ctx.hasActiveBindings))
            return CommandResult(false, "No parameters/bindings");
        
        auto param = ctx.parameters[0];
        foreach(binding; cSelectedBindings.values) {
            binding.interpolateMode = mode;
        }
        incViewportNodeDeformNotifyParamValueChanged();
        return CommandResult(true);
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

void ngInitCommands(T)() if (is(T == BindingCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!BindingCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); } ))
            mixin(registerCommand!(name));
    }
    mixin(registerCommand!(BindingCommand.SetInterpolation, InterpolateMode.Linear));

    // Also ensure providers are registered once this module initializes
    import nijigenerate.commands.binding.base : ngInitBindingProviders;
    ngInitBindingProviders();
}
