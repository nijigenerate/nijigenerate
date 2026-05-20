module nijigenerate.actions.depthbone;

import nijigenerate.actions;
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

    this(string label, ExDepthRigRoot root, ExDepthRigBinding[] oldBindings, ExDepthRigBinding[] newBindings) {
        this.label = label;
        this.root = root;
        this.oldBindings = oldBindings.dup;
        this.newBindings = newBindings.dup;
        root.notifyChange(root, NotifyReason.AttributeChanged);
    }

    void rollback() {
        root.bindings = oldBindings.dup;
        root.notifyChange(root, NotifyReason.AttributeChanged);
    }

    void redo() {
        root.bindings = newBindings.dup;
        root.notifyChange(root, NotifyReason.AttributeChanged);
    }

    string describe() { return label; }
    string describeUndo() { return label; }
    string getName() { return label; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

alias DepthBoneSourceListChangeAction = DepthRigBindingsChangeAction;
alias DepthBoneBindingRuleChangeAction = DepthRigBindingsChangeAction;

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
        bone.notifyChange(bone, NotifyReason.AttributeChanged);
    }

    void apply(vec3 head, vec3 tail, float roll) {
        bone.restHead = head;
        bone.restTail = tail;
        bone.restRoll = roll;
        bone.notifyChange(bone, NotifyReason.AttributeChanged);
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
    float[] oldRotationLimits;
    float oldMaxStepRadians;
    string newConstraintType;
    vec3 newHingeAxis;
    bool newLockRotation;
    bool newLockTranslation;
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
        oldRotationLimits = bone.rotationLimits.dup;
        oldMaxStepRadians = bone.maxStepRadians;
    }

    void updateNewState() {
        newConstraintType = bone.constraintType;
        newHingeAxis = bone.hingeAxis;
        newLockRotation = bone.lockRotation;
        newLockTranslation = bone.lockTranslation;
        newRotationLimits = bone.rotationLimits.dup;
        newMaxStepRadians = bone.maxStepRadians;
        bone.notifyChange(bone, NotifyReason.AttributeChanged);
    }

    void apply(string constraintType, vec3 hingeAxis, bool lockRotation, bool lockTranslation, float[] rotationLimits, float maxStepRadians) {
        bone.constraintType = constraintType;
        bone.hingeAxis = hingeAxis;
        bone.lockRotation = lockRotation;
        bone.lockTranslation = lockTranslation;
        bone.rotationLimits = rotationLimits.dup;
        bone.maxStepRadians = maxStepRadians;
        bone.notifyChange(bone, NotifyReason.AttributeChanged);
    }

    void rollback() { apply(oldConstraintType, oldHingeAxis, oldLockRotation, oldLockTranslation, oldRotationLimits, oldMaxStepRadians); }
    void redo() { apply(newConstraintType, newHingeAxis, newLockRotation, newLockTranslation, newRotationLimits, newMaxStepRadians); }
    string describe() { return _("Depth bone constraint changed"); }
    string describeUndo() { return _("Depth bone constraint changed"); }
    string getName() { return "DepthBoneConstraintChangeAction"; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}
