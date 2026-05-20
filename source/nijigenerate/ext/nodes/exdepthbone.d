/*
    Depth bone rig extension nodes.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.ext.nodes.exdepthbone;

import nijilive.core;
import nijilive.core.nodes;
import nijilive.fmt.serialize;
import nijilive.math;

import std.algorithm.searching : countUntil;
import std.exception : enforce;

enum ExDepthTargetKind {
    Grid,
    Path,
}

string depthTargetKindName(ExDepthTargetKind kind) {
    final switch (kind) {
        case ExDepthTargetKind.Grid: return "grid";
        case ExDepthTargetKind.Path: return "path";
    }
}

ExDepthTargetKind parseDepthTargetKind(string value) {
    switch (value) {
        case "grid": return ExDepthTargetKind.Grid;
        case "path": return ExDepthTargetKind.Path;
        default: throw new Exception("unknown depth rig target kind: " ~ value);
    }
}

struct ExDepthInfluenceRule {
    uint maxInfluences = 4;
    float radiusScale = 1.0f;
    float minimumRadius = 1.0f;
    string falloff = "gaussian";
    float[ulong] multipliersByBoneUuid;

    void serialize(S)(ref S serializer) const {
        auto state = serializer.structBegin();
        serializer.putKey("maxInfluences");
        serializer.serializeValue(maxInfluences);
        serializer.putKey("radiusScale");
        serializer.serializeValue(radiusScale);
        serializer.putKey("minimumRadius");
        serializer.serializeValue(minimumRadius);
        serializer.putKey("falloff");
        serializer.serializeValue(falloff);
        if (multipliersByBoneUuid.length > 0) {
            serializer.putKey("multipliersByBoneUuid");
            serializer.serializeValue(multipliersByBoneUuid);
        }
        serializer.structEnd(state);
    }

    SerdeException deserializeFromFghj(Fghj data) {
        if (!data["maxInfluences"].isEmpty) {
            if (auto exc = data["maxInfluences"].deserializeValue(maxInfluences)) return exc;
        }
        if (!data["radiusScale"].isEmpty) {
            if (auto exc = data["radiusScale"].deserializeValue(radiusScale)) return exc;
        }
        if (!data["minimumRadius"].isEmpty) {
            if (auto exc = data["minimumRadius"].deserializeValue(minimumRadius)) return exc;
        }
        if (!data["falloff"].isEmpty) {
            if (auto exc = data["falloff"].deserializeValue(falloff)) return exc;
        }
        if (!data["multipliersByBoneUuid"].isEmpty) {
            if (auto exc = data["multipliersByBoneUuid"].deserializeValue(multipliersByBoneUuid)) return exc;
        }
        if (maxInfluences == 0) maxInfluences = 1;
        return null;
    }
}

struct ExDepthBoneSourceSettings {
    ulong boneUuid;
    float weight = 1.0f;
    float depthOffset = 0.0f;
    float depthScale = 1.0f;

    void serialize(S)(ref S serializer) const {
        auto state = serializer.structBegin();
        serializer.putKey("bone");
        serializer.serializeValue(boneUuid);
        serializer.putKey("weight");
        serializer.serializeValue(weight);
        serializer.putKey("depthOffset");
        serializer.serializeValue(depthOffset);
        serializer.putKey("depthScale");
        serializer.serializeValue(depthScale);
        serializer.structEnd(state);
    }

    SerdeException deserializeFromFghj(Fghj data) {
        if (!data["bone"].isEmpty) {
            if (auto exc = data["bone"].deserializeValue(boneUuid)) return exc;
        }
        if (!data["weight"].isEmpty) {
            if (auto exc = data["weight"].deserializeValue(weight)) return exc;
        }
        if (!data["depthOffset"].isEmpty) {
            if (auto exc = data["depthOffset"].deserializeValue(depthOffset)) return exc;
        }
        if (!data["depthScale"].isEmpty) {
            if (auto exc = data["depthScale"].deserializeValue(depthScale)) return exc;
        }
        return null;
    }
}

struct ExDepthRigBinding {
    ulong targetUuid;
    ExDepthTargetKind targetKind = ExDepthTargetKind.Grid;
    ulong[] sourceBoneUuids;
    ExDepthBoneSourceSettings[] sourceSettings;
    ExDepthInfluenceRule influenceRule;

    ptrdiff_t findSourceSettingIndex(ulong boneUuid) const {
        foreach (i, ref setting; sourceSettings) {
            if (setting.boneUuid == boneUuid) return cast(ptrdiff_t)i;
        }
        return -1;
    }

    ExDepthBoneSourceSettings sourceSetting(ulong boneUuid) const {
        auto index = findSourceSettingIndex(boneUuid);
        if (index >= 0) return sourceSettings[cast(size_t)index];

        ExDepthBoneSourceSettings setting;
        setting.boneUuid = boneUuid;
        return setting;
    }

    void normalizeSourceSettings() {
        ExDepthBoneSourceSettings[] normalized;
        foreach (uuid; sourceBoneUuids) {
            normalized ~= sourceSetting(uuid);
        }
        sourceSettings = normalized;
    }

    void setSourceSetting(ExDepthBoneSourceSettings setting) {
        auto index = sourceBoneUuids.countUntil(setting.boneUuid);
        if (index < 0) sourceBoneUuids ~= setting.boneUuid;

        normalizeSourceSettings();
        auto settingIndex = findSourceSettingIndex(setting.boneUuid);
        if (settingIndex >= 0) sourceSettings[cast(size_t)settingIndex] = setting;
    }

    void serialize(S)(ref S serializer) const {
        auto state = serializer.structBegin();
        serializer.putKey("target");
        serializer.serializeValue(targetUuid);
        serializer.putKey("targetKind");
        serializer.putValue(depthTargetKindName(targetKind));
        serializer.putKey("sourceBoneUuids");
        serializer.serializeValue(sourceBoneUuids);
        if (sourceSettings.length > 0) {
            serializer.putKey("sourceSettings");
            serializer.serializeValue(sourceSettings);
        }
        serializer.putKey("influenceRule");
        influenceRule.serialize(serializer);
        serializer.structEnd(state);
    }

    SerdeException deserializeFromFghj(Fghj data) {
        if (!data["target"].isEmpty) {
            if (auto exc = data["target"].deserializeValue(targetUuid)) return exc;
        }
        if (!data["targetKind"].isEmpty) {
            string kind;
            if (auto exc = data["targetKind"].deserializeValue(kind)) return exc;
            try {
                targetKind = parseDepthTargetKind(kind);
            } catch (Exception e) {
                return new SerdeException(e.msg);
            }
        }
        if (!data["sourceBoneUuids"].isEmpty) {
            if (auto exc = data["sourceBoneUuids"].deserializeValue(sourceBoneUuids)) return exc;
        }
        if (!data["sourceSettings"].isEmpty) {
            if (auto exc = data["sourceSettings"].deserializeValue(sourceSettings)) return exc;
        }
        if (!data["influenceRule"].isEmpty) {
            if (auto exc = influenceRule.deserializeFromFghj(data["influenceRule"])) return exc;
        }
        normalizeSourceSettings();
        return null;
    }
}

@TypeId("DepthBone")
class ExDepthBone : Node {
public:
    string boneId;
    vec3 restHead = vec3(0, 0, 0);
    vec3 restTail = vec3(0, 100, 0);
    float restRoll = 0.0f;

    string constraintType;
    vec3 hingeAxis = vec3(0, 0, 1);
    bool lockRotation = false;
    bool lockTranslation = false;
    bool allowParentToTargets = true;
    float[] rotationLimits;
    float maxStepRadians = 0.0f;

    this(Node parent = null) {
        super(parent);
    }

    override
    string typeId() {
        return "DepthBone";
    }

protected:
    override
    void serializeSelfImpl(ref InochiSerializer serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags.All) {
        super.serializeSelfImpl(serializer, recursive, flags);

        if (flags & SerializeNodeFlags.State) {
            serializer.putKey("boneId");
            serializer.serializeValue(boneId);
            serializer.putKey("restHead");
            restHead.serialize(serializer);
            serializer.putKey("restTail");
            restTail.serialize(serializer);
            serializer.putKey("restRoll");
            serializer.serializeValue(restRoll);

            if (constraintType.length > 0) {
                serializer.putKey("constraintType");
                serializer.serializeValue(constraintType);
            }
            serializer.putKey("hingeAxis");
            hingeAxis.serialize(serializer);
            serializer.putKey("lockRotation");
            serializer.serializeValue(lockRotation);
            serializer.putKey("lockTranslation");
            serializer.serializeValue(lockTranslation);
            serializer.putKey("allowParentToTargets");
            serializer.serializeValue(allowParentToTargets);
            if (rotationLimits.length > 0) {
                serializer.putKey("rotationLimits");
                serializer.serializeValue(rotationLimits);
            }
            if (maxStepRadians != 0.0f) {
                serializer.putKey("maxStepRadians");
                serializer.serializeValue(maxStepRadians);
            }
        }
    }

    override
    SerdeException deserializeFromFghj(Fghj data) {
        if (auto exc = super.deserializeFromFghj(data)) return exc;

        if (!data["boneId"].isEmpty) {
            if (auto exc = data["boneId"].deserializeValue(boneId)) return exc;
        }
        if (!data["restHead"].isEmpty) restHead.deserialize(data["restHead"]);
        if (!data["restTail"].isEmpty) restTail.deserialize(data["restTail"]);
        if (!data["restRoll"].isEmpty) {
            if (auto exc = data["restRoll"].deserializeValue(restRoll)) return exc;
        }
        if (!data["constraintType"].isEmpty) {
            if (auto exc = data["constraintType"].deserializeValue(constraintType)) return exc;
        }
        if (!data["hingeAxis"].isEmpty) hingeAxis.deserialize(data["hingeAxis"]);
        if (!data["lockRotation"].isEmpty) {
            if (auto exc = data["lockRotation"].deserializeValue(lockRotation)) return exc;
        }
        if (!data["lockTranslation"].isEmpty) {
            if (auto exc = data["lockTranslation"].deserializeValue(lockTranslation)) return exc;
        }
        if (!data["allowParentToTargets"].isEmpty) {
            if (auto exc = data["allowParentToTargets"].deserializeValue(allowParentToTargets)) return exc;
        }
        if (!data["rotationLimits"].isEmpty) {
            if (auto exc = data["rotationLimits"].deserializeValue(rotationLimits)) return exc;
        }
        if (!data["maxStepRadians"].isEmpty) {
            if (auto exc = data["maxStepRadians"].deserializeValue(maxStepRadians)) return exc;
        }
        return null;
    }
}

@TypeId("DepthRigRoot")
class ExDepthRigRoot : Node {
public:
    ExDepthRigBinding[] bindings;

    this(Node parent = null) {
        super(parent);
    }

    override
    string typeId() {
        return "DepthRigRoot";
    }

    ExDepthBone[] depthBones() {
        ExDepthBone[] result;

        void visit(Node n) {
            if (auto bone = cast(ExDepthBone)n) result ~= bone;
            foreach (child; n.children) visit(child);
        }

        foreach (child; children) visit(child);
        return result;
    }

    ptrdiff_t findBindingIndex(ulong targetUuid) const {
        foreach (i, ref binding; bindings) {
            if (binding.targetUuid == targetUuid) return cast(ptrdiff_t)i;
        }
        return -1;
    }

    ExDepthRigBinding* getOrCreateBinding(Node target, ExDepthTargetKind kind) {
        auto index = findBindingIndex(target.uuid);
        if (index >= 0) return &bindings[cast(size_t)index];

        ExDepthRigBinding binding;
        binding.targetUuid = target.uuid;
        binding.targetKind = kind;
        bindings ~= binding;
        return &bindings[$ - 1];
    }

    void addBoneSource(Node target, ExDepthTargetKind kind, ExDepthBone bone) {
        enforce(target !is null, "target is required");
        enforce(bone !is null, "bone is required");
        auto binding = getOrCreateBinding(target, kind);
        if (binding.sourceBoneUuids.countUntil(bone.uuid) < 0) {
            binding.sourceBoneUuids ~= bone.uuid;
        }
        binding.normalizeSourceSettings();
    }

    void removeBoneSource(Node target, ExDepthBone bone) {
        enforce(target !is null, "target is required");
        enforce(bone !is null, "bone is required");
        auto index = findBindingIndex(target.uuid);
        if (index < 0) return;
        auto binding = &bindings[cast(size_t)index];
        auto sourceIndex = binding.sourceBoneUuids.countUntil(bone.uuid);
        if (sourceIndex >= 0) {
            import std.algorithm.mutation : remove;
            binding.sourceBoneUuids = binding.sourceBoneUuids.remove(cast(size_t)sourceIndex);
        }
        binding.normalizeSourceSettings();
    }

protected:
    override
    void serializeSelfImpl(ref InochiSerializer serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags.All) {
        super.serializeSelfImpl(serializer, recursive, flags);

        if ((flags & SerializeNodeFlags.Links) && bindings.length > 0) {
            serializer.putKey("bindings");
            serializer.serializeValue(bindings);
        }
    }

    override
    SerdeException deserializeFromFghj(Fghj data) {
        if (auto exc = super.deserializeFromFghj(data)) return exc;

        bindings.length = 0;
        if (!data["bindings"].isEmpty) {
            foreach (entry; data["bindings"].byElement) {
                ExDepthRigBinding binding;
                if (auto exc = binding.deserializeFromFghj(entry)) return exc;
                bindings ~= binding;
            }
        }
        return null;
    }
}

ExDepthBone incCreateDepthBone(Node parent, string boneId, vec3 restHead, vec3 restTail, float restRoll = 0.0f) {
    auto bone = new ExDepthBone(parent);
    bone.name = boneId;
    bone.boneId = boneId;
    bone.restHead = restHead;
    bone.restTail = restTail;
    bone.restRoll = restRoll;
    if (auto parentBone = cast(ExDepthBone)parent) {
        bone.localTransform.translation = restHead - parentBone.restHead;
    } else {
        bone.localTransform.translation = restHead;
    }
    bone.localTransform.update();
    bone.transformChanged();
    return bone;
}

void incAddStandardDepthSkeleton(ExDepthRigRoot root, float scale = 1.0f) {
    enforce(root !is null, "Depth rig root is required");

    vec4 bounds;
    bool hasBounds = false;
    if (root.parent !is null) {
        bounds = root.parent.getCombinedBounds!(true)();
        auto size = bounds.zw - bounds.xy;
        hasBounds = size.x > 1e-4f && size.y > 1e-4f;
    }
    if (!hasBounds) {
        auto halfWidth = 180.0f * scale;
        auto halfHeight = 300.0f * scale;
        bounds = vec4(-halfWidth, -halfHeight, halfWidth, halfHeight);
    }

    auto rootToLocal = root.transform.matrix.inverse;
    auto width = (bounds.z - bounds.x) * scale;
    auto height = (bounds.w - bounds.y) * scale;
    auto cx = (bounds.x + bounds.z) * 0.5f;
    auto top = bounds.y;
    auto bottom = bounds.w;

    vec3 p(float xRatio, float yRatio, float z = 0) {
        auto world = vec3(cx + xRatio * width, top + yRatio * (bottom - top) * scale, z * height);
        return (rootToLocal * vec4(world.x, world.y, world.z, 1)).xyz;
    }

    auto pelvis = incCreateDepthBone(root, "Pelvis", p(0, 0.58f), p(0, 0.47f));
    auto spine = incCreateDepthBone(pelvis, "Spine", p(0, 0.47f), p(0, 0.34f));
    auto chest = incCreateDepthBone(spine, "Chest", p(0, 0.34f), p(0, 0.22f));
    auto neck = incCreateDepthBone(chest, "Neck", p(0, 0.22f), p(0, 0.16f));
    incCreateDepthBone(neck, "Head", p(0, 0.16f), p(0, 0.06f));

    auto clavicleL = incCreateDepthBone(chest, "Clavicle.L", p(0, 0.25f), p(-0.14f, 0.25f));
    auto upperArmL = incCreateDepthBone(clavicleL, "UpperArm.L", p(-0.14f, 0.25f), p(-0.26f, 0.42f));
    auto forearmL = incCreateDepthBone(upperArmL, "Forearm.L", p(-0.26f, 0.42f), p(-0.31f, 0.58f));
    incCreateDepthBone(forearmL, "Hand.L", p(-0.31f, 0.58f), p(-0.33f, 0.64f));

    auto clavicleR = incCreateDepthBone(chest, "Clavicle.R", p(0, 0.25f), p(0.14f, 0.25f));
    auto upperArmR = incCreateDepthBone(clavicleR, "UpperArm.R", p(0.14f, 0.25f), p(0.26f, 0.42f));
    auto forearmR = incCreateDepthBone(upperArmR, "Forearm.R", p(0.26f, 0.42f), p(0.31f, 0.58f));
    incCreateDepthBone(forearmR, "Hand.R", p(0.31f, 0.58f), p(0.33f, 0.64f));

    auto thighL = incCreateDepthBone(pelvis, "Thigh.L", p(-0.08f, 0.58f), p(-0.12f, 0.76f));
    auto shinL = incCreateDepthBone(thighL, "Shin.L", p(-0.12f, 0.76f), p(-0.12f, 0.94f));
    incCreateDepthBone(shinL, "Foot.L", p(-0.12f, 0.94f), p(-0.18f, 0.98f));

    auto thighR = incCreateDepthBone(pelvis, "Thigh.R", p(0.08f, 0.58f), p(0.12f, 0.76f));
    auto shinR = incCreateDepthBone(thighR, "Shin.R", p(0.12f, 0.76f), p(0.12f, 0.94f));
    incCreateDepthBone(shinR, "Foot.R", p(0.12f, 0.94f), p(0.18f, 0.98f));
}

void incRegisterExDepthBoneNodes() {
    inRegisterNodeType!ExDepthRigRoot();
    inRegisterNodeType!ExDepthBone();
}
