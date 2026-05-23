module nijigenerate.panels.inspector.depthbone;

import nijigenerate;
import nijigenerate.commands;
import nijigenerate.commands.depth.bone : ngMarkDepthBoneDirtyAllKeypointsForArmedParameter;
import nijigenerate.ext;
import nijigenerate.panels.inspector.common;
import nijigenerate.widgets;
import nijilive;
import i18n;
import std.conv;
import std.string;

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: ExDepthRigRoot) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    override
    void run() {
        if (targets.length == 0) return;
        auto root = targets[0];
        if (incBeginCategory(__("Depth Rig"))) {
            igText(__("Bones: %d"), cast(int)root.depthBones().length);
            igText(__("Bindings: %d"), cast(int)root.bindings.length);
            if (igButton(__("Add Standard Skeleton"))) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(DepthBoneCommand.AddStandardDepthSkeleton)(ctx, root, 1.0f);
            }
            if (igButton(__("Add Standard Parameters"))) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(DepthBoneCommand.AddStandardDepthParameters)(ctx, root);
            }
        }
        incEndCategory();
    }
}

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: ExDepthBone) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    override
    void run() {
        if (targets.length == 0) return;
        auto bone = targets[0];
        if (incBeginCategory(__("Depth Bone"))) {
            igText("%s", bone.boneId.toStringz);
            if (incButtonColored("\ue5d5")) {
                Node cursor = bone;
                while (cursor !is null) {
                    if (auto root = cast(ExDepthRigRoot)cursor) {
                        ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(root, "Force Depth Bone Refresh");
                        break;
                    }
                    cursor = cursor.parent;
                }
            }
            incTooltip(_("Force refresh all Depth Bone target keypoints"));

            float[3] head = [bone.restHead.x, bone.restHead.y, bone.restHead.z];
            float[3] tail = [bone.restTail.x, bone.restTail.y, bone.restTail.z];
            float roll = bone.restRoll;
            bool changed = false;

            changed = igDragFloat3("Rest Head", &head, 1.0f) || changed;
            changed = igDragFloat3("Rest Tail", &tail, 1.0f) || changed;
            changed = igDragFloat("Rest Roll", &roll, 0.01f) || changed;

            if (changed) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(DepthBoneCommand.SetDepthBoneRest)(ctx, bone, [head[0], head[1], head[2]], [tail[0], tail[1], tail[2]], roll);
            }

            bool lockRot = bone.lockRotation;
            bool lockTrans = bone.lockTranslation;
            bool allowParentToTargets = bone.allowParentToTargets;
            bool constraintChanged = false;
            constraintChanged = ngCheckbox(__("Lock Rotation"), &lockRot) || constraintChanged;
            constraintChanged = ngCheckbox(__("Lock Translation"), &lockTrans) || constraintChanged;
            constraintChanged = ngCheckbox(__("Allow Parent to Targets"), &allowParentToTargets) || constraintChanged;
            incTooltip(_("When enabled, parent bone rotation and translation also affect targets bound to this bone."));
            if (constraintChanged) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(DepthBoneCommand.SetDepthBoneConstraint)(
                    ctx,
                    bone,
                    `{"lockRotation":` ~ (lockRot ? "true" : "false")
                    ~ `,"lockTranslation":` ~ (lockTrans ? "true" : "false")
                    ~ `,"allowParentToTargets":` ~ (allowParentToTargets ? "true" : "false")
                    ~ `}`
                );
            }
        }
        incEndCategory();
    }
}
