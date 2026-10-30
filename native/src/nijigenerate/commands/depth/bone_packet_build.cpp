#include "bone_packet_build.hpp"
#include "bone_constants.hpp"
#include "detail/bone_packet_source.hpp"
#include "detail/bone_fit.hpp"
#include "detail/bone_hash.hpp"
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <limits>

namespace nijigenerate::commands::depth::bone {

bool ngBuildDepthBoneGpuOffsetPacket(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        ext::nodes::ExDepthRigBinding* binding, std::shared_ptr<nicxlive::core::nodes::Deformable> target,
        std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u cursor,
        DepthBoneGpuOffsetPacket& packet, compat::RetainedStringSlice& error,
        bool writePreview, bool writeBinding) {
    // Retain real binding backing before output resets or virtual callbacks can
    // release native owners. Subsequent reads still use the original live slot.
    [[maybe_unused]] const auto bindingOwner = root ? root->bindings
        : compat::Slice<ext::nodes::ExDepthRigBinding>{};
    packet = DepthBoneGpuOffsetPacket{};
    error.reset();
    if (!root) {
        error = compat::RetainedString::fromStaticLiteral("Depth rig root is required");
        return false;
    }
    if (!binding) {
        error = compat::RetainedString::fromStaticLiteral("Depth rig binding is required");
        return false;
    }
    if (!target) {
        error = compat::RetainedString::fromStaticLiteral("target is not deformable");
        return false;
    }
    if (binding->sourceBoneUuids.empty()) {
        error = compat::RetainedString::fromStaticLiteral("Depth rig binding has no bone sources");
        return false;
    }
    if (target->getVertices().size() > DepthBoneGpuMaxVertices) {
        error = compat::RetainedString::fromStaticLiteral("Depth bone GPU packet exceeds maximum vertex count");
        return false;
    }
    compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>> sourceBones;
    auto sourceUuids = binding->sourceBoneUuids;
    for (auto uuid : sourceUuids) {
        if (auto bone = detail::findBoneByUuid(root, uuid)) sourceBones.append(bone);
    }
    if (sourceBones.empty()) {
        error = compat::RetainedString::fromStaticLiteral("No valid depth bone sources");
        return false;
    }

    auto runtime = detail::buildDepthRigRuntime(root, parameter, cursor);
    detail::DepthBoneIndexMap boneIndices;
    compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>> runtimeBones;
    auto depthBones = root->depthBones();
    for (auto bone : depthBones) {
        if (!runtime.find(bone->getUUID())) continue;
        if (runtimeBones.size() >= DepthBoneGpuMaxBones) {
            error = compat::RetainedString::fromStaticLiteral("Depth bone GPU packet exceeds maximum bone count");
            return false;
        }
        boneIndices.set(bone->getUUID(), static_cast<std::uint32_t>(runtimeBones.size()));
        runtimeBones.append(bone);
    }
    if (sourceBones.size() > DepthBoneGpuMaxSources) {
        error = compat::RetainedString::fromStaticLiteral("Depth bone GPU packet exceeds maximum source count");
        return false;
    }
    const auto maxInfluences = binding->influenceRule.maxInfluences == 0
        ? 1u : binding->influenceRule.maxInfluences;
    if (maxInfluences > DepthBoneGpuMaxInfluences) {
        error = compat::RetainedString::fromStaticLiteral("Depth bone GPU packet exceeds maximum influence count");
        return false;
    }

    compat::Slice<float> boneData;
    for (auto bone : runtimeBones) {
        auto runtimeBone = runtime.find(bone->getUUID());
        if (!runtimeBone) continue;
        detail::appendVec3(boneData, (*runtimeBone)->restHead);
        boneData.append((*runtimeBone)->restLength);
        detail::appendVec3(boneData, (*runtimeBone)->restTail);
        float parentIndex = -1.0f;
        if ((*runtimeBone)->parent && (*runtimeBone)->parent->source) {
            if (auto parent = boneIndices.find((*runtimeBone)->parent->source->getUUID()))
                parentIndex = static_cast<float>(*parent);
        }
        boneData.append(parentIndex);
        detail::appendMat4(boneData, (*runtimeBone)->skinMatrix);
    }
    const auto worldScale = detail::depthWorldScale(root);
    auto rawDepths = detail::snapshotTargetDepths(target);
    compat::Slice<float> scaledDepths;
    scaledDepths.resize(rawDepths.size(), std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < rawDepths.size(); ++i) scaledDepths[i] = rawDepths[i] * worldScale;
    auto targetNode = std::static_pointer_cast<nicxlive::core::nodes::Node>(target);
    const auto targetToRoot = detail::targetToRootMatrix(root, targetNode);

    compat::Slice<float> sourceData;
    compat::Slice<float> sourceInputs;
    if (!detail::appendDepthBoneGpuSourceData(binding, sourceBones, boneIndices, runtime,
            target, scaledDepths, targetToRoot, worldScale, sourceData, sourceInputs, error)) return false;

    packet.root = root;
    packet.target = target;
    packet.parameter = parameter;
    packet.keypoint = cursor;
    packet.writePreview = writePreview;
    packet.writeBinding = writeBinding;
    packet.vertices = target->getVertices().dup();
    packet.rawDepths = rawDepths;
    packet.depths = scaledDepths;
    packet.targetToRoot = targetToRoot;
    packet.rootToTarget = core::math::source_matrix::inverse(packet.targetToRoot);
    packet.worldScale = worldScale;
    const auto minimumRadius = binding->influenceRule.minimumRadius;
    const auto boundsRadius = detail::targetBoundsSize(target) * 0.18f;
    packet.influenceRadiusFloor = minimumRadius < boundsRadius ? boundsRadius : minimumRadius;
    packet.radiusScale = binding->influenceRule.radiusScale;
    packet.maxInfluences = maxInfluences;
    packet.bones = boneData;
    packet.sourceInputs = sourceInputs;
    packet.sources = sourceData;
    packet.boneCount = static_cast<std::uint32_t>(runtimeBones.size());
    packet.sourceCount = static_cast<std::uint32_t>(sourceBones.size());
    packet.rigHash = detail::depthBoneRigStructureHash(root);
    packet.parameterStructureHash = parameter ? detail::depthBoneParameterStructureHash(root, parameter) : 0;
    packet.poseHash = parameter ? detail::depthBonePoseKeyHash(root, parameter, cursor) : 0;
    return true;
}

} // namespace nijigenerate::commands::depth::bone
