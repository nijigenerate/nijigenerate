module nijigenerate.actions.depthbone;

import nijigenerate.actions;
import nijigenerate.actions.depthboneinvalidation : ngNotifyDepthBoneRigChanged;
import nijigenerate.ext.nodes.exdepthbone;
import nijilive;
import nijilive.math;

import i18n;
import std.format : format;

class DepthRigBindingsChangeAction : Action {
    ExDepthRigRoot root;
    ExDepthRigBinding[] oldBindings;
    ExDepthRigBinding[] newBindings;
    string label;
    bool settleBeforeDispatch;
    Node affectedTarget;

    private static ExDepthRigBinding[] copyBindings(ExDepthRigBinding[] bindings) {
        auto result = bindings.dup;
        foreach (ref binding; result) {
            binding.sourceBoneUuids = binding.sourceBoneUuids.dup;
            binding.sourceSettings = binding.sourceSettings.dup;
            binding.influenceRule.multipliersByBoneUuid = binding.influenceRule.multipliersByBoneUuid.dup;
        }
        return result;
    }

    this(
        string label,
        ExDepthRigRoot root,
        ExDepthRigBinding[] oldBindings,
        ExDepthRigBinding[] newBindings,
        bool settleBeforeDispatch = false,
        Node affectedTarget = null,
    ) {
        this.label = label;
        this.root = root;
        this.oldBindings = copyBindings(oldBindings);
        this.newBindings = copyBindings(newBindings);
        this.settleBeforeDispatch = settleBeforeDispatch;
        this.affectedTarget = affectedTarget;
        notifyChanged();
    }

    private void notifyChanged() {
        root.notifyChange(root, NotifyReason.AttributeChanged);
        ngNotifyDepthBoneRigChanged(
            root, label, settleBeforeDispatch, affectedTarget);
    }

    void rollback() {
        root.bindings = copyBindings(oldBindings);
        notifyChanged();
    }

    void redo() {
        root.bindings = copyBindings(newBindings);
        notifyChanged();
    }

    string describe() { return label; }
    string describeUndo() { return label; }
    string getName() { return label; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

alias DepthBoneSourceListChangeAction = DepthRigBindingsChangeAction;
alias DepthBoneBindingRuleChangeAction = DepthRigBindingsChangeAction;

/**
 * Applies every translation Z changed by one Fit Z to Depth operation as a
 * single semantic mutation.  A root fit can touch many bones, but generated
 * deform output depends on the completed rig pose, not on its intermediate
 * per-bone states.
 */
class DepthBoneFitZChangeAction : Action {
    ExDepthRigRoot root;
    ExDepthBone[] bones;
    float[] oldValues;
    float[] newValues;

    this(
        ExDepthRigRoot root,
        ExDepthBone[] bones,
        float[] oldValues,
        float[] newValues,
    ) {
        assert(root !is null);
        assert(bones.length == oldValues.length);
        assert(bones.length == newValues.length);
        this.root = root;
        this.bones = bones.dup;
        this.oldValues = oldValues.dup;
        this.newValues = newValues.dup;
        notifyChanged();
    }

    private void notifyChanged() {
        foreach (bone; bones) {
            if (bone !is null)
                bone.notifyChange(bone, NotifyReason.AttributeChanged);
        }
        ngNotifyDepthBoneRigChanged(root, "Fit Z to Depth");
    }

    private void apply(float[] values) {
        foreach (i, bone; bones) {
            if (bone is null) continue;
            bone.localTransform.translation.vector[2] = values[i];
            bone.localTransform.update();
            bone.transformChanged();
        }
        notifyChanged();
    }

    void rollback() { apply(oldValues); }
    void redo() { apply(newValues); }
    string describe() { return _("Fit Z to Depth"); }
    string describeUndo() { return _("Fit Z to Depth"); }
    string getName() { return "DepthBoneFitZChangeAction"; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

class DepthBoneRestChangeAction : Action {
    ExDepthBone bone;
    vec3 oldHead;
    vec3 oldTail;
    float oldRoll;
    vec3 newHead;
    vec3 newTail;
    float newRoll;

    this(ExDepthBone bone, vec3 oldHead, vec3 oldTail, float oldRoll, vec3 newHead, vec3 newTail, float newRoll) {
        this.bone = bone;
        this.oldHead = oldHead;
        this.oldTail = oldTail;
        this.oldRoll = oldRoll;
        this.newHead = newHead;
        this.newTail = newTail;
        this.newRoll = newRoll;
        notifyChanged();
    }

    private void notifyChanged() {
        bone.notifyChange(bone, NotifyReason.AttributeChanged);
        ngNotifyDepthBoneRigChanged(bone, "Depth Bone Rest");
    }

    void apply(vec3 head, vec3 tail, float roll) {
        bone.restHead = head;
        bone.restTail = tail;
        bone.restRoll = roll;
        notifyChanged();
    }

    void rollback() { apply(oldHead, oldTail, oldRoll); }
    void redo() { apply(newHead, newTail, newRoll); }
    string describe() { return _("Depth bone rest changed"); }
    string describeUndo() { return _("Depth bone rest changed"); }
    string getName() { return "DepthBoneRestChangeAction"; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

class DepthBoneConstraintChangeAction : Action {
    ExDepthBone bone;
    string oldConstraintType;
    vec3 oldHingeAxis;
    bool oldLockRotation;
    bool oldLockTranslation;
    bool oldAllowParentToTargets;
    float[] oldRotationLimits;
    float oldMaxStepRadians;
    string newConstraintType;
    vec3 newHingeAxis;
    bool newLockRotation;
    bool newLockTranslation;
    bool newAllowParentToTargets;
    float[] newRotationLimits;
    float newMaxStepRadians;

    this(ExDepthBone bone) {
        this.bone = bone;
        captureOld();
    }

    void captureOld() {
        oldConstraintType = bone.constraintType;
        oldHingeAxis = bone.hingeAxis;
        oldLockRotation = bone.lockRotation;
        oldLockTranslation = bone.lockTranslation;
        oldAllowParentToTargets = bone.allowParentToTargets;
        oldRotationLimits = bone.rotationLimits.dup;
        oldMaxStepRadians = bone.maxStepRadians;
    }

    void updateNewState() {
        newConstraintType = bone.constraintType;
        newHingeAxis = bone.hingeAxis;
        newLockRotation = bone.lockRotation;
        newLockTranslation = bone.lockTranslation;
        newAllowParentToTargets = bone.allowParentToTargets;
        newRotationLimits = bone.rotationLimits.dup;
        newMaxStepRadians = bone.maxStepRadians;
        notifyChanged();
    }

    private void notifyChanged() {
        bone.notifyChange(bone, NotifyReason.AttributeChanged);
        ngNotifyDepthBoneRigChanged(bone, "Depth Bone Constraint");
    }

    void apply(string constraintType, vec3 hingeAxis, bool lockRotation, bool lockTranslation, bool allowParentToTargets, float[] rotationLimits, float maxStepRadians) {
        bone.constraintType = constraintType;
        bone.hingeAxis = hingeAxis;
        bone.lockRotation = lockRotation;
        bone.lockTranslation = lockTranslation;
        bone.allowParentToTargets = allowParentToTargets;
        bone.rotationLimits = rotationLimits.dup;
        bone.maxStepRadians = maxStepRadians;
        notifyChanged();
    }

    void rollback() { apply(oldConstraintType, oldHingeAxis, oldLockRotation, oldLockTranslation, oldAllowParentToTargets, oldRotationLimits, oldMaxStepRadians); }
    void redo() { apply(newConstraintType, newHingeAxis, newLockRotation, newLockTranslation, newAllowParentToTargets, newRotationLimits, newMaxStepRadians); }
    string describe() { return _("Depth bone constraint changed"); }
    string describeUndo() { return _("Depth bone constraint changed"); }
    string getName() { return "DepthBoneConstraintChangeAction"; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}
