#include "detail/bone_dirty_providers.hpp"
#include <nijigenerate/project.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/deformable.hpp>
#include <param/binding.hpp>

namespace nijigenerate::actions::node::detail {
// Existing real production read-access fault provider, already required by the
// actual NodeValue actions. Keep one provider rather than a successful skip or
// an undefined C++ dereference. The M012 source unit is not modified here.
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::commands::depth::bone {
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::nodes::GridDeformer;
using nicxlive::core::param::Parameter;

namespace detail {

compat::RetainedStringSlice dirtyScopeName(DepthBoneDirtyScope scope) {
    if (scope == DepthBoneDirtyScope::AllKeypoints)
        return compat::RetainedString::fromStaticLiteral("all-keypoints");
    return compat::RetainedString::fromStaticLiteral("keypoint");
}

bool rootContainsBone(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<ExDepthBone> bone) {
    if (!root || !bone) return false;
    std::shared_ptr<Node> cursor = bone;
    while (cursor) {
        if (cursor == root) return true;
        cursor = cursor->parentRef();
    }
    return false;
}

bool depthBoneParameterDrivesRig(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Parameter> parameter) {
    if (!root || !parameter) return false;
    auto bindings = parameter->bindings;
    for (auto binding : bindings) {
        if (!binding) actions::node::detail::failActionObjectReference();
        auto bone = std::dynamic_pointer_cast<ExDepthBone>(binding->getTarget().getResource());
        if (!bone || !rootContainsBone(root, bone)) continue;
        if (isDepthBoneTransformBindingName(compat::RetainedString(binding->getNameDescriptor()))) return true;
    }
    return false;
}

std::shared_ptr<Parameter> depthBoneRefreshParameter(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> candidate) {
    return depthBoneParameterDrivesRig(root, candidate) ? candidate : nullptr;
}

bool depthBoneTargetMatchesScope(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> candidate,
        std::uint32_t targetUuid) {
    if (targetUuid == 0) return true;
    auto puppet = project::incActivePuppet();
    if (!root || !puppet || !candidate) return false;
    auto scopeTarget = puppet->findNodeById(targetUuid);
    if (!scopeTarget) return false;
    if (candidate == scopeTarget) return true;
    if (!isSameOrAncestorNode(scopeTarget, candidate)) return false;
    if (auto parentGrid = std::dynamic_pointer_cast<GridDeformer>(scopeTarget)) {
        auto deformable = std::dynamic_pointer_cast<Deformable>(candidate);
        return deformable && parentGridInfluenceCanReachTarget(root, parentGrid, deformable);
    }
    return true;
}

bool depthBoneBindingMatchesTarget(std::shared_ptr<ExDepthRigRoot> root, ext::nodes::ExDepthRigBinding& binding,
        std::uint32_t targetUuid) {
    if (targetUuid == 0) return true;
    auto puppet = project::incActivePuppet();
    if (!puppet) return false;
    auto candidate = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
    return depthBoneTargetMatchesScope(root, candidate, targetUuid);
}

} // namespace detail

void ngMarkDepthBoneDirtyForTarget(std::shared_ptr<Node> target, compat::RetainedStringSlice reason) {
    using namespace detail;
    if (!target || !project::incActivePuppet()) return;
    if (auto bone = std::dynamic_pointer_cast<ExDepthBone>(target)) {
        if (auto root = findDepthRigRoot(bone)) {
            if (!depthBoneRootHasGeneratedTargets(root)) return;
            if (depthBoneAffectedParameters(root).empty()) return;
            ngMarkDepthBoneDirty(root, nullptr, compat::Vec2u{}, reason, DepthBoneDirtyScope::AllKeypoints);
        }
        return;
    }
    if (auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(target)) {
        if (!depthBoneRootHasGeneratedTargets(root)) return;
        if (depthBoneAffectedParameters(root).empty()) return;
        ngMarkDepthBoneDirty(root, nullptr, compat::Vec2u{}, reason, DepthBoneDirtyScope::AllKeypoints);
        return;
    }
    for (auto root : depthBoneRoots()) {
        if (!depthBoneRootHasGeneratedTargets(root)) continue;
        bool affectsTargetScope = false;
        auto bindings = root->bindings;
        for (auto& binding : bindings) {
            if (depthBoneBindingMatchesTarget(root, binding, static_cast<std::uint32_t>(target->getUUID()))) {
                affectsTargetScope = true;
                break;
            }
        }
        if (!affectsTargetScope) continue;
        if (depthBoneAffectedParameters(root).empty()) continue;
        ngMarkDepthBoneDirty(root, nullptr, compat::Vec2u{}, reason, DepthBoneDirtyScope::AllKeypoints,
            static_cast<std::uint32_t>(target->getUUID()));
    }
}

} // namespace nijigenerate::commands::depth::bone
