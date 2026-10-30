#include "depthbone_publishers.hpp"
#include "depthbone_mutation_hook.hpp"
#include "depthbone_publisher_assertion.hpp"

namespace nijigenerate::actions::depthboneinvalidation {

void ngNotifyDepthBoneBindingValueChanged(
        std::shared_ptr<nicxlive::core::param::ParameterBinding> binding, compat::Vec2u keypoint) {
    if (!binding || !ngDepthBoneMutationHook) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind::BindingValue;
    mutation.binding = binding;
    mutation.keypoint = keypoint;
    mutation.reason = compat::RetainedString::fromStaticLiteral("Parameter Binding Value");
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneBindingAllValuesChanged(
        std::shared_ptr<nicxlive::core::param::ParameterBinding> binding) {
    if (!binding || !ngDepthBoneMutationHook) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind::BindingAllValues;
    mutation.binding = binding;
    mutation.reason = compat::RetainedString::fromStaticLiteral("Parameter Binding Values");
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneBindingStructureChanged(
        std::shared_ptr<nicxlive::core::param::ParameterBinding> binding) {
    if (!binding || !ngDepthBoneMutationHook) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind::BindingStructure;
    mutation.binding = binding;
    mutation.reason = compat::RetainedString::fromStaticLiteral("Parameter Binding Structure");
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneRigChanged(
        std::shared_ptr<nicxlive::core::nodes::Node> target,
        compat::RetainedStringSlice reason, bool settleBeforeDispatch,
        std::shared_ptr<nicxlive::core::nodes::Node> affectedTarget) {
    if (!target || !ngDepthBoneMutationHook) return;
    DepthBoneMutation mutation;
    mutation.kind = DepthBoneMutationKind::RigConfiguration;
    mutation.target = target;
    mutation.affectedTarget = affectedTarget;
    mutation.reason = reason;
    mutation.settleBeforeDispatch = settleBeforeDispatch;
    ngDepthBoneMutationHook(mutation);
}

void ngNotifyDepthBoneTargetChanged(
        std::shared_ptr<nicxlive::core::nodes::Node> target,
        DepthBoneMutationKind kind, compat::RetainedStringSlice reason) {
    if (!target || !ngDepthBoneMutationHook) return;
#ifndef NDEBUG
    if (kind != DepthBoneMutationKind::TargetTransform && kind != DepthBoneMutationKind::TargetGeometry &&
            kind != DepthBoneMutationKind::TargetDepth)
        throw compat::ngStaticError<DepthBoneMutationAssertion>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/depthboneinvalidation.d"), 86);
#endif
    DepthBoneMutation mutation;
    mutation.kind = kind;
    mutation.target = target;
    mutation.reason = reason;
    ngDepthBoneMutationHook(mutation);
}

} // namespace nijigenerate::actions::depthboneinvalidation
