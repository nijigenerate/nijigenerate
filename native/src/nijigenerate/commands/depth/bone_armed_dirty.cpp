#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_refresh_state.hpp"
#include <nijigenerate/project.hpp>

namespace nijigenerate::commands::depth::bone {

void ngMarkDepthBoneDirtyForArmedParameter(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        compat::RetainedStringSlice reason, DepthBoneDirtyScope dirtyScope) {
    using namespace detail;
    auto parameter = depthBoneRefreshParameter(root, project::incArmedParameter());
    compat::Vec2u keypoint{};
    if (parameter) {
        const auto closest = parameter->findClosestKeypoint();
        keypoint = {closest.x, closest.y};
    }
    if (!parameter && lastDepthBoneDirtyRoot == root)
        parameter = depthBoneRefreshParameter(root, lastDepthBoneDirtyParameter);
    if (parameter && lastDepthBoneDirtyRoot == root && parameter == lastDepthBoneDirtyParameter)
        keypoint = lastDepthBoneDirtyKeypoint;
    ngMarkDepthBoneDirty(root, parameter, keypoint, reason, dirtyScope);
}

void ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        compat::RetainedStringSlice reason) {
    ngMarkDepthBoneDirtyForArmedParameter(root, reason, DepthBoneDirtyScope::AllKeypoints);
}

} // namespace nijigenerate::commands::depth::bone
