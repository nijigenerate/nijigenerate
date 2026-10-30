#pragma once

#include "../bone.hpp"
#include <nijigenerate/actions/depthbone_mutation.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <param/parameter.hpp>

namespace nicxlive::core::param {
class ValueParameterBinding;
class DeformationParameterBinding;
}
namespace nicxlive::core::nodes { class GridDeformer; class Deformable; }

namespace nijigenerate::commands::depth::bone {

namespace detail {

void markDepthBoneUpdateTargets(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    std::uint32_t targetUuid, compat::RetainedStringSlice reason);
void setDepthBoneUpdateOwner(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::uint32_t targetUuid, actions::AsyncActionToken token);
void planDepthBoneUpdateTargets(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    std::uint32_t targetUuid, compat::RetainedStringSlice reason, std::size_t expectedWork);
void failDepthBoneUpdateTargets(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    std::uint32_t targetUuid, compat::RetainedStringSlice reason, compat::RetainedStringSlice detail);

// Shared declarations for actual query and downstream dirty implementations.
// Missing producers remain unresolved; no no-op may make dispatch link.
compat::Slice<std::shared_ptr<ext::nodes::ExDepthRigRoot>> depthBoneRoots();
bool isLiveDepthRigRoot(std::shared_ptr<ext::nodes::ExDepthRigRoot> root);
compat::RetainedStringSlice keypointKey(compat::Vec2u keypoint);
compat::Slice<compat::Vec2u> depthBoneKeypoints(std::shared_ptr<nicxlive::core::param::Parameter> parameter);
std::shared_ptr<ext::nodes::ExDepthRigRoot> findDepthRigRoot(std::shared_ptr<ext::nodes::ExDepthBone> bone);
bool hasValidDepthBoneSources(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    ext::nodes::ExDepthRigBinding& binding);
bool depthBoneRootHasGeneratedTargets(std::shared_ptr<ext::nodes::ExDepthRigRoot> root);
compat::Slice<std::shared_ptr<nicxlive::core::param::Parameter>> depthBoneAffectedParameters(
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root);
bool isSameOrAncestorNode(std::shared_ptr<nicxlive::core::nodes::Node> ancestor,
    std::shared_ptr<nicxlive::core::nodes::Node> node);
compat::RetainedStringSlice dirtyScopeName(DepthBoneDirtyScope scope);
bool rootContainsBone(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone);
bool depthBoneParameterDrivesRig(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter);
std::shared_ptr<nicxlive::core::param::Parameter> depthBoneRefreshParameter(
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> candidate);
bool depthBoneTargetMatchesScope(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::nodes::Node> candidate, std::uint32_t targetUuid);
bool depthBoneBindingMatchesTarget(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    ext::nodes::ExDepthRigBinding& binding, std::uint32_t targetUuid);
bool depthBoneSourceIncludesAncestorPose(std::shared_ptr<ext::nodes::ExDepthBone> targetSource,
    std::shared_ptr<ext::nodes::ExDepthBone> ancestorSource);
bool parentGridInfluenceCanReachTarget(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::nodes::GridDeformer> parentGrid,
    std::shared_ptr<nicxlive::core::nodes::Deformable> target);
bool ngMarkDepthBoneDirtyForTransformBindingTarget(std::shared_ptr<nicxlive::core::nodes::Node> changedNode,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    compat::RetainedStringSlice reason);
bool ngMarkDepthBoneDirtyForDeformationBindingTarget(std::shared_ptr<nicxlive::core::nodes::Node> changedNode,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    DepthBoneDirtyScope dirtyScope, compat::RetainedStringSlice reason);

bool depthBoneBindingMutationInfo(std::shared_ptr<nicxlive::core::param::ParameterBinding> binding,
    std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
    std::shared_ptr<nicxlive::core::nodes::Node>& target, compat::RetainedStringSlice& bindingName);
bool isDepthBoneTransformBindingName(compat::RetainedStringSlice name);

} // namespace detail
} // namespace nijigenerate::commands::depth::bone
