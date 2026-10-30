#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_math.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/deformable.hpp>

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::GridDeformer;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::param::Parameter;

namespace {
std::shared_ptr<Node> currentBindingTarget(std::uint64_t uuid) {
    auto puppet = project::incActivePuppet();
    return compat::ngRequireObject(puppet)->findNodeById(static_cast<std::uint32_t>(uuid));
}
}

bool depthBoneSourceIncludesAncestorPose(std::shared_ptr<ExDepthBone> targetSource,
        std::shared_ptr<ExDepthBone> ancestorSource) {
    if (!targetSource || !ancestorSource) return false;
    auto cursor = targetSource;
    while (cursor) {
        if (cursor == ancestorSource) return true;
        if (!cursor->allowParentToTargets || cursor->lockToRootValue()) return false;
        cursor = std::dynamic_pointer_cast<ExDepthBone>(cursor->parentRef());
    }
    return false;
}

bool parentGridInfluenceCanReachTarget(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<GridDeformer> parentGrid, std::shared_ptr<Deformable> target) {
    if (!root || !parentGrid || !target) return false;
    const auto parentIndex = root->findBindingIndex(parentGrid->getUUID());
    const auto targetIndex = root->findBindingIndex(target->getUUID());
    if (parentIndex < 0 || targetIndex < 0) return false;
    // Retain the actual binding allocation containing the two addressed cells.
    // This copies its descriptor, not bindings or a surrogate source table.
    auto bindings = root->bindings;
    auto* parentBinding = &bindings[static_cast<std::size_t>(parentIndex)];
    auto* targetBinding = &bindings[static_cast<std::size_t>(targetIndex)];
    auto parentSources = parentBinding->sourceBoneUuids;
    for (auto parentUuid : parentSources) {
        auto parentSource = findBoneByUuid(root, parentUuid);
        if (!parentSource) continue;
        auto targetSources = targetBinding->sourceBoneUuids;
        for (auto targetUuid : targetSources) {
            auto targetSource = findBoneByUuid(root, targetUuid);
            if (depthBoneSourceIncludesAncestorPose(targetSource, parentSource)) return true;
        }
    }
    return false;
}

bool ngMarkDepthBoneDirtyForTransformBindingTarget(std::shared_ptr<Node> changedNode,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint, compat::RetainedStringSlice reason) {
    if (!changedNode || !parameter || !project::incActivePuppet()) return false;
    bool marked = false;
    for (auto root : depthBoneRoots()) {
        if (!depthBoneRootHasGeneratedTargets(root)) continue;
        bool affectsRoot = false;
        auto bindings = root->bindings;
        for (auto& binding : bindings) {
            auto target = currentBindingTarget(binding.targetUuid);
            if (!target) continue;
            if (changedNode == target || isSameOrAncestorNode(changedNode, target) || changedNode == root) {
                affectsRoot = true;
                break;
            }
        }
        if (!affectsRoot) continue;
        ngMarkDepthBoneDirty(root, parameter, keypoint, reason, DepthBoneDirtyScope::Keypoint);
        marked = true;
    }
    return marked;
}

bool ngMarkDepthBoneDirtyForDeformationBindingTarget(std::shared_ptr<Node> changedNode,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint, DepthBoneDirtyScope scope,
        compat::RetainedStringSlice reason) {
    auto parentGrid = std::dynamic_pointer_cast<GridDeformer>(changedNode);
    if (!parentGrid || !parameter || !project::incActivePuppet()) return false;
    bool marked = false;
    for (auto root : depthBoneRoots()) {
        if (!depthBoneRootHasGeneratedTargets(root)) continue;
        bool affectsDescendant = false;
        auto bindings = root->bindings;
        for (auto& binding : bindings) {
            auto targetNode = currentBindingTarget(binding.targetUuid);
            auto target = std::dynamic_pointer_cast<Deformable>(targetNode);
            if (!targetNode || !target || targetNode == changedNode) continue;
            if (!isSameOrAncestorNode(changedNode, targetNode)) continue;
            if (!parentGridInfluenceCanReachTarget(root, parentGrid, target)) continue;
            affectsDescendant = true;
            break;
        }
        if (!affectsDescendant) continue;
        // Parent and descendants must share one hierarchy generation batch.
        ngMarkDepthBoneDirty(root, parameter, keypoint, reason, scope);
        marked = true;
    }
    return marked;
}

} // namespace nijigenerate::commands::depth::bone::detail
