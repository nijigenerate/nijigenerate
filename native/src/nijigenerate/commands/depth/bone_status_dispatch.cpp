#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_refresh_state.hpp"
#include "bone_status.hpp"
#include <nijigenerate/project.hpp>

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthRigRoot;
using nicxlive::core::param::Parameter;

void markDepthBoneUpdateTargets(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint,
        std::uint32_t targetUuid, compat::RetainedStringSlice reason) {
    auto puppet = project::incActivePuppet();
    if (!root || !puppet) return;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        if (!depthBoneBindingMatchesTarget(root, binding, targetUuid)) continue;
        if (!hasValidDepthBoneSources(root, binding)) continue;
        auto target = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
        if (!target) continue;
        bone_status::ngDepthBoneUpdateDetected(root, target, parameter, keypoint, reason.retainedString());
        if (depthBoneRefreshActionSink)
            bone_status::ngDepthBoneUpdateSetOwnerToken(root, target, depthBoneRefreshActionSink->asyncToken());
    }
}

void setDepthBoneUpdateOwner(std::shared_ptr<ExDepthRigRoot> root,
        std::uint32_t targetUuid, actions::AsyncActionToken token) {
    auto puppet = project::incActivePuppet();
    if (!root || !puppet || !token.valid()) return;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        if (!depthBoneBindingMatchesTarget(root, binding, targetUuid)) continue;
        auto target = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
        if (target) bone_status::ngDepthBoneUpdateSetOwnerToken(root, target, token);
    }
}

void planDepthBoneUpdateTargets(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint,
        std::uint32_t targetUuid, compat::RetainedStringSlice reason, std::size_t expectedWork) {
    auto puppet = project::incActivePuppet();
    if (!root || !puppet || expectedWork == 0) return;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        if (!depthBoneBindingMatchesTarget(root, binding, targetUuid)) continue;
        if (!hasValidDepthBoneSources(root, binding)) continue;
        auto target = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
        if (!target) continue;
        bone_status::ngDepthBoneUpdatePlanned(root, target, parameter, keypoint, reason.retainedString(), expectedWork);
    }
}

void failDepthBoneUpdateTargets(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint,
        std::uint32_t targetUuid, compat::RetainedStringSlice reason, compat::RetainedStringSlice detail) {
    auto puppet = project::incActivePuppet();
    if (!root || !puppet) return;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        if (!depthBoneBindingMatchesTarget(root, binding, targetUuid)) continue;
        if (!hasValidDepthBoneSources(root, binding)) continue;
        auto target = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
        if (!target) continue;
        bone_status::ngDepthBoneUpdateFailed(root, target, parameter, keypoint,
            reason.retainedString(), detail.retainedString());
    }
}

} // namespace nijigenerate::commands::depth::bone::detail
