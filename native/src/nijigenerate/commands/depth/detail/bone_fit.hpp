#pragma once

#include "bone_dirty_providers.hpp"
#include "bone_math.hpp"
#include <nijigenerate/compat/const_slice.hpp>

namespace nijigenerate::commands::depth::bone {

namespace detail {

BoneQuaternion depthEditRotation(float pitch, float yaw, float roll);
nicxlive::core::math::Vec3 depthBoneNodeRestPosition(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::nodes::Node> node);
std::shared_ptr<ext::nodes::ExDepthBone> firstDepthBoneChild(std::shared_ptr<ext::nodes::ExDepthBone> bone);
nicxlive::core::math::Vec3 depthBonePlanarDirection(nicxlive::core::math::Vec3 direction);
void effectiveDepthBoneRest(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone, nicxlive::core::math::Vec3& head,
    nicxlive::core::math::Vec3& tail);

float depthWorldScale(std::shared_ptr<ext::nodes::ExDepthRigRoot> root);
float depthBoneSourceTangentSlope(float rotation);
nicxlive::core::math::Vec3 depthBoneSourceRestLocal(float x, float y, float rawDepth,
    ext::nodes::ExDepthBoneSourceSettings setting, float depthUnitScale = 1.0f);
float depthBoneSourceRotationPivotXShift(std::shared_ptr<nicxlive::core::nodes::Deformable> target,
    compat::ConstSlice<float> scaledDepths, nicxlive::core::math::Mat4 targetToRoot,
    nicxlive::core::math::Vec3 boneRestHead, ext::nodes::ExDepthBoneSourceSettings setting, float worldScale);
bool nearestScaledWorldDepthAtPoint(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone, nicxlive::core::math::Vec2 worldPoint, float& worldDepth);
bool fitDepthBoneTranslationZToCurrentDepth(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone,
    compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>>& changedBones,
    compat::Slice<float>& oldValues, compat::Slice<float>& newValues);
bool fitDepthBoneTranslationZ(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone, float localTranslationZ,
    compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>>& changedBones,
    compat::Slice<float>& oldValues, compat::Slice<float>& newValues);
std::shared_ptr<nicxlive::core::param::Parameter> depthFitParameter(
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root);
bool fitDepthBoneBindingZToCurrentDepth(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone, std::shared_ptr<nicxlive::core::param::Parameter> parameter,
    compat::Vec2u keypoint, std::shared_ptr<actions::GroupAction> group, bool& changed);
bool fitOneDepthBoneZToDepth(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone,
    compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>>& changedBones,
    compat::Slice<float>& oldValues, compat::Slice<float>& newValues);

} // namespace detail
} // namespace nijigenerate::commands::depth::bone
