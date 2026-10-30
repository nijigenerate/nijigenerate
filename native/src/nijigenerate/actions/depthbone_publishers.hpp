#pragma once

#include "depthbone_mutation.hpp"

namespace nijigenerate::actions::depthboneinvalidation {

void ngNotifyDepthBoneBindingValueChanged(
    std::shared_ptr<nicxlive::core::param::ParameterBinding> binding, compat::Vec2u keypoint);
void ngNotifyDepthBoneBindingAllValuesChanged(
    std::shared_ptr<nicxlive::core::param::ParameterBinding> binding);
void ngNotifyDepthBoneBindingStructureChanged(
    std::shared_ptr<nicxlive::core::param::ParameterBinding> binding);
void ngNotifyDepthBoneRigChanged(
    std::shared_ptr<nicxlive::core::nodes::Node> target,
    compat::RetainedStringSlice reason,
    bool settleBeforeDispatch = false,
    std::shared_ptr<nicxlive::core::nodes::Node> affectedTarget = nullptr);
void ngNotifyDepthBoneTargetChanged(
    std::shared_ptr<nicxlive::core::nodes::Node> target,
    DepthBoneMutationKind kind,
    compat::RetainedStringSlice reason);

} // namespace nijigenerate::actions::depthboneinvalidation
