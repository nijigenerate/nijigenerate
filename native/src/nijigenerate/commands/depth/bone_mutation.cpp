#include "detail/bone_startup_hooks.hpp"
#include "detail/bone_dirty_providers.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <param/binding_impl.hpp>

namespace nijigenerate::commands::depth::bone {
using nicxlive::core::nodes::Node;
using nicxlive::core::param::Parameter;
using nicxlive::core::param::ParameterBinding;
using nicxlive::core::param::ValueParameterBinding;
using nicxlive::core::param::DeformationParameterBinding;
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;
using actions::depthboneinvalidation::DepthBoneMutation;
using actions::depthboneinvalidation::DepthBoneMutationKind;

namespace detail {
bool depthBoneBindingMutationInfo(std::shared_ptr<ParameterBinding> binding,
        std::shared_ptr<Parameter>& parameter, std::shared_ptr<Node>& target,
        compat::RetainedStringSlice& bindingName) {
    parameter.reset();
    target.reset();
    bindingName.reset();
    if (auto value = std::dynamic_pointer_cast<ValueParameterBinding>(binding)) {
        parameter = value->getParameter();
        target = std::dynamic_pointer_cast<Node>(value->getTarget().getResource());
        bindingName = compat::RetainedString(value->getNameDescriptor());
        return parameter && target;
    }
    if (auto deform = std::dynamic_pointer_cast<DeformationParameterBinding>(binding)) {
        parameter = deform->getParameter();
        target = std::dynamic_pointer_cast<Node>(deform->getTarget().getResource());
        bindingName = compat::RetainedString(deform->getNameDescriptor());
        return parameter && target;
    }
    return false;
}

bool isDepthBoneTransformBindingName(compat::RetainedStringSlice name) {
    return name.view().starts_with("transform.t.") || name.view().starts_with("transform.r.") ||
        name.view().starts_with("transform.s.");
}
} // namespace detail

namespace {
template<std::size_t N> compat::RetainedStringSlice literalReason(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
}

void ngDepthBoneMutationChanged(DepthBoneMutation mutation) {
    using namespace detail;
    switch (mutation.kind) {
    case DepthBoneMutationKind::BindingValue:
    case DepthBoneMutationKind::BindingAllValues:
    case DepthBoneMutationKind::BindingStructure: {
        std::shared_ptr<Parameter> parameter;
        std::shared_ptr<Node> target;
        compat::RetainedStringSlice bindingName;
        if (!depthBoneBindingMutationInfo(mutation.binding, parameter, target, bindingName)) return;
        const auto scope = mutation.kind == DepthBoneMutationKind::BindingValue
            ? DepthBoneDirtyScope::Keypoint : DepthBoneDirtyScope::AllKeypoints;
        if (std::dynamic_pointer_cast<DeformationParameterBinding>(mutation.binding)) {
            ngMarkDepthBoneDirtyForDeformationBindingTarget(target, parameter, mutation.keypoint, scope,
                literalReason("Depth Bone Ancestor Deformation"));
            return;
        }
        if (!isDepthBoneTransformBindingName(bindingName)) return;
        if (auto bone = std::dynamic_pointer_cast<ExDepthBone>(target)) {
            auto root = findDepthRigRoot(bone);
            if (root && depthBoneRootHasGeneratedTargets(root)) {
                ngMarkDepthBoneDirty(root, parameter, mutation.keypoint,
                    literalReason("Depth Bone Transform"), scope);
            }
            return;
        }
        if (scope == DepthBoneDirtyScope::Keypoint) {
            ngMarkDepthBoneDirtyForTransformBindingTarget(target, parameter, mutation.keypoint,
                literalReason("Depth Bone Target Transform"));
        } else {
            for (auto root : depthBoneRoots()) {
                if (!depthBoneRootHasGeneratedTargets(root)) continue;
                bool affectsRoot = target == root;
                auto bindings = root->bindings;
                for (auto& binding : bindings) {
                    // Preserve the repeated live active-puppet lookup and uint
                    // narrowing, rather than using a cached puppet or UUID set.
                    auto puppet = project::incActivePuppet();
                    auto targetNode = compat::ngRequireObject(puppet)->findNodeById(
                        static_cast<std::uint32_t>(binding.targetUuid));
                    if (targetNode && (target == targetNode || isSameOrAncestorNode(target, targetNode))) {
                        affectsRoot = true;
                        break;
                    }
                }
                if (affectsRoot) {
                    ngMarkDepthBoneDirty(root, parameter, mutation.keypoint,
                        literalReason("Depth Bone Target Transform"), DepthBoneDirtyScope::AllKeypoints);
                }
            }
        }
        return;
    }
    case DepthBoneMutationKind::RigConfiguration: {
        std::shared_ptr<ExDepthRigRoot> root;
        if (auto direct = std::dynamic_pointer_cast<ExDepthRigRoot>(mutation.target)) root = direct;
        else if (auto bone = std::dynamic_pointer_cast<ExDepthBone>(mutation.target)) root = findDepthRigRoot(bone);
        if (root) {
            if (!depthBoneRootHasGeneratedTargets(root)) return;
            if (depthBoneAffectedParameters(root).empty()) return;
            ngMarkDepthBoneDirty(root, nullptr, compat::Vec2u{}, mutation.reason, DepthBoneDirtyScope::AllKeypoints,
                mutation.affectedTarget ? static_cast<std::uint32_t>(mutation.affectedTarget->getUUID()) : 0,
                mutation.settleBeforeDispatch);
        }
        return;
    }
    case DepthBoneMutationKind::TargetTransform:
    case DepthBoneMutationKind::TargetGeometry:
    case DepthBoneMutationKind::TargetDepth:
        ngMarkDepthBoneDirtyForTarget(mutation.target, mutation.reason);
        return;
    }
    throw compat::SwitchError(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), 2950);
}

} // namespace nijigenerate::commands::depth::bone
