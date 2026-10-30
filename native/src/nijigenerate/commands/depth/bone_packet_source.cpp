#include "detail/bone_packet_source.hpp"
#include "detail/bone_fit.hpp"
#include "bone_constants.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <cmath>

namespace nijigenerate::commands::depth::bone::detail {

bool appendDepthBoneGpuSourceData(ext::nodes::ExDepthRigBinding* binding,
        compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>> sourceBones,
        DepthBoneIndexMap boneIndices, RuntimeDepthBoneMap runtime,
        std::shared_ptr<nicxlive::core::nodes::Deformable> target,
        compat::Slice<float> scaledDepths, nicxlive::core::math::Mat4 targetToRoot, float worldScale,
        compat::Slice<float>& sourceData, compat::Slice<float>& sourceInputs, compat::RetainedStringSlice& error) {
    for (auto bone : sourceBones) {
        auto boneIndex = boneIndices.find(compat::ngRequireObject(bone)->getUUID());
        auto runtimeBone = runtime.find(compat::ngRequireObject(bone)->getUUID());
        if (!boneIndex || !runtimeBone) {
            error = compat::RetainedString::fromStaticLiteral("Depth bone runtime is missing");
            return false;
        }
        auto setting = compat::ngRequireObject(binding)->sourceSetting(bone->getUUID());
        float multiplier = 1.0f;
        if (auto value = binding->influenceRule.multipliersByBoneUuid.find(bone->getUUID())) multiplier = *value;
        const auto sourceStart = sourceInputs.size();
        sourceInputs.append(static_cast<float>(*boneIndex));
        sourceInputs.append(isTerminalDepthBoneSource(bone, sourceBones) ? 1.0f : 0.0f);
        sourceInputs.append(bone->lockToRootValue() ? 1.0f : 0.0f);
        sourceInputs.append(binding->influenceRule.falloff.view() == "linear" ? 1.0f : 0.0f);
        sourceInputs.append(setting.weight);
        sourceInputs.append(setting.depthScale);
        sourceInputs.append(setting.depthOffset);
        sourceInputs.append(multiplier);
        const float rotation = ext::nodes::ngNormalizeDepthBoneSourceRotation(setting.rotation);
        sourceInputs.append(static_cast<float>(std::sin(rotation)));
        sourceInputs.append(static_cast<float>(std::cos(rotation)));
        const auto sourceDataStart = sourceData.size();
        sourceData.append(sourceInputs.slice(sourceStart, sourceInputs.size()));
        sourceData[sourceDataStart + DepthBoneGpuSourceDepthOffsetIndex] *= worldScale;
        sourceData.append(depthBoneSourceRotationPivotXShift(target, scaledDepths, targetToRoot,
            compat::ngRequireObject(*runtimeBone)->restHead, setting, worldScale));
        sourceData.append(compat::ngRequireObject(*runtimeBone)->poseYaw);
        appendMat4(sourceData, depthBoneNoYawSkinMatrix(*runtimeBone));
    }
    return true;
}

} // namespace nijigenerate::commands::depth::bone::detail
