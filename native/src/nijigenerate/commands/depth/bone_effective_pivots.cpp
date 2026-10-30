#include "bone_effective_pivots.hpp"
#include "detail/bone_runtime.hpp"
#include "detail/bone_fit.hpp"
#include <nijigenerate/project.hpp>
#include <algorithm>
#include <cmath>

namespace nijigenerate::commands::depth::bone {

compat::Slice<DepthBoneSourceEffectivePivot> ngDepthBoneSourceEffectivePivots(
        std::shared_ptr<ext::nodes::ExDepthRigRoot> root, std::shared_ptr<ext::nodes::ExDepthBone> bone) {
    compat::Slice<DepthBoneSourceEffectivePivot> result;
    auto puppet = project::incActivePuppet();
    if (!root || !bone || !puppet || !puppet->root) return result;
    [[maybe_unused]] const auto modelRootLifetime = puppet->root;
    auto runtime = detail::buildDepthRigRuntime(root);
    auto runtimeBone = runtime.find(bone->getUUID());
    if (!runtimeBone) return result;
    const auto worldScale = detail::depthWorldScale(root);
    const auto noYawSkin = detail::depthBoneNoYawSkinMatrix(*runtimeBone);
    const auto bonePoint = detail::transformPoint(noYawSkin, (*runtimeBone)->restHead);
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        const auto sourceUuids = binding.sourceBoneUuids;
        const auto sourceUuid = bone->getUUID();
        if (std::find(sourceUuids.begin(), sourceUuids.end(), sourceUuid) == sourceUuids.end()) continue;
        auto targetNode = detail::findNodeByUuid(puppet->root, binding.targetUuid);
        auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(targetNode);
        if (!target) continue;
        auto rawDepths = detail::snapshotTargetDepths(target);
        compat::Slice<float> scaledDepths;
        scaledDepths.resize(rawDepths.size(), std::numeric_limits<float>::quiet_NaN());
        for (std::size_t i = 0; i < rawDepths.size(); ++i) scaledDepths[i] = rawDepths[i] * worldScale;
        const auto targetToRoot = detail::targetToRootMatrix(root, targetNode);
        auto setting = binding.sourceSetting(bone->getUUID());
        const auto shift = detail::depthBoneSourceRotationPivotXShift(target, scaledDepths,
            targetToRoot, (*runtimeBone)->restHead, setting, worldScale);
        if (!std::isfinite(shift)) continue;
        DepthBoneSourceEffectivePivot pivot;
        pivot.targetUuid = binding.targetUuid;
        pivot.bonePoint = bonePoint;
        pivot.effectivePoint = detail::transformPoint(noYawSkin,
            {(*runtimeBone)->restHead.x - shift, (*runtimeBone)->restHead.y - 0.0f,
                (*runtimeBone)->restHead.z - 0.0f});
        pivot.rotationPivotXShift = shift;
        pivot.rotation = setting.rotation;
        result.append(pivot);
    }
    return result;
}

} // namespace nijigenerate::commands::depth::bone
