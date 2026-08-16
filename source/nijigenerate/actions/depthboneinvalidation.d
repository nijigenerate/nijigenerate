module nijigenerate.actions.depthboneinvalidation;

import nijilive;
import nijilive.math : vec2u;

/**
 * Semantic model mutations which can invalidate generated DepthBone output.
 *
 * Action and editor implementations publish only the mutation that occurred;
 * the DepthBone subsystem is the single owner of dependency and dirty-scope
 * resolution. This keeps command, viewport, and undo/redo paths identical.
 */
enum DepthBoneMutationKind {
    BindingValue,
    BindingAllValues,
    BindingStructure,
    RigConfiguration,
    TargetTransform,
    TargetGeometry,
    TargetDepth,
}

struct DepthBoneMutation {
    DepthBoneMutationKind kind;
    Node target;
    Node affectedTarget;
    ParameterBinding binding;
    vec2u keypoint;
    string reason;
    bool settleBeforeDispatch;
}

alias DepthBoneMutationHook = void function(DepthBoneMutation mutation);
__gshared DepthBoneMutationHook ngDepthBoneMutationHook;

void ngNotifyDepthBoneBindingValueChanged(ParameterBinding binding, vec2u keypoint) {
    if (binding is null || ngDepthBoneMutationHook is null) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind.BindingValue;
    mutation.binding = binding;
    mutation.keypoint = keypoint;
    mutation.reason = "Parameter Binding Value";
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneBindingAllValuesChanged(ParameterBinding binding) {
    if (binding is null || ngDepthBoneMutationHook is null) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind.BindingAllValues;
    mutation.binding = binding;
    mutation.reason = "Parameter Binding Values";
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneBindingStructureChanged(ParameterBinding binding) {
    if (binding is null || ngDepthBoneMutationHook is null) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind.BindingStructure;
    mutation.binding = binding;
    mutation.reason = "Parameter Binding Structure";
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneRigChanged(
    Node target,
    string reason,
    bool settleBeforeDispatch = false,
    Node affectedTarget = null,
) {
    if (target is null || ngDepthBoneMutationHook is null) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind.RigConfiguration;
    mutation.target = target;
    mutation.affectedTarget = affectedTarget;
    mutation.reason = reason;
    mutation.settleBeforeDispatch = settleBeforeDispatch;
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneTargetChanged(
    Node target,
    DepthBoneMutationKind kind,
    string reason,
) {
    if (target is null || ngDepthBoneMutationHook is null) return;
    assert(kind == DepthBoneMutationKind.TargetTransform ||
        kind == DepthBoneMutationKind.TargetGeometry ||
        kind == DepthBoneMutationKind.TargetDepth);
    DepthBoneMutation mutation;
    mutation.kind = kind;
    mutation.target = target;
    mutation.reason = reason;
    ngDepthBoneMutationHook(mutation);
}
