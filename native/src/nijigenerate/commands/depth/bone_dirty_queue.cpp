// Dirty request coalescing and TLS state from the independent depth bone module.
#include "detail/bone_dirty_state.hpp"
#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_pivot_cache.hpp"
#include "detail/bone_debug_log.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::commands::depth::bone::detail {
thread_local compat::Slice<DepthBoneDirtyRequest> depthBoneDirtyRequests;
bool sameDirtyParameter(DepthBoneDirtyRequest request, std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, std::uint32_t targetUuid) {
    return request.root == root && request.parameter == parameter && request.targetUuid == targetUuid &&
        request.actionSink == depthBoneRefreshActionSink;
}
}
namespace nijigenerate::commands::depth::bone {
void ngMarkDepthBoneDirty(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    compat::RetainedStringSlice reason, DepthBoneDirtyScope scope, std::uint32_t targetUuid, bool settle) {
    using namespace detail;
    if (!root) return;
    ngInvalidateDepthBoneEffectivePivotCache(root);
    if (depthBoneRefreshSuppressed) return;
    markDepthBoneUpdateTargets(root, parameter, keypoint, targetUuid, reason);
    if (parameter) {
        lastDepthBoneDirtyRoot = root; lastDepthBoneDirtyParameter = parameter; lastDepthBoneDirtyKeypoint = keypoint;
    }
    {
        const auto rootName = root->getName();
        const auto parameterName = parameter ? parameter->getName()
            : nicxlive::core::common::NullableString::fromStaticLiteral("(none)");
        depthBoneDebugLog("[DepthBoneRefresh] mark: root=%s param=%s key=(%s,%s) scope=%s reason=%s",
            rootName, parameterName, keypoint.x, keypoint.y, dirtyScopeName(scope), reason);
    }
    for (auto requests = depthBoneDirtyRequests; auto& request : requests) {
        if (!sameDirtyParameter(request, root, parameter, targetUuid)) continue;
        if (request.dirtyScope == DepthBoneDirtyScope::AllKeypoints || scope == DepthBoneDirtyScope::AllKeypoints) {
            request.dirtyScope = DepthBoneDirtyScope::AllKeypoints; request.keypoint = keypoint;
            request.settleBeforeDispatch = request.settleBeforeDispatch || settle;
            if (!reason.empty()) request.reason = reason;
            const auto rootName = root->getName();
            const auto parameterName = parameter ? parameter->getName()
                : nicxlive::core::common::NullableString::fromStaticLiteral("(none)");
            depthBoneDebugLog("[DepthBoneRefresh] mark merged: root=%s param=%s key=(%s,%s) scope=%s reason=%s",
                rootName, parameterName, keypoint.x, keypoint.y,
                dirtyScopeName(request.dirtyScope), request.reason);
            return;
        }
        if (request.keypoint == keypoint) {
            if (!reason.empty()) request.reason = reason;
            const auto rootName = root->getName();
            const auto parameterName = parameter ? parameter->getName()
                : nicxlive::core::common::NullableString::fromStaticLiteral("(none)");
            depthBoneDebugLog("[DepthBoneRefresh] mark merged: root=%s param=%s key=(%s,%s) scope=%s reason=%s",
                rootName, parameterName, keypoint.x, keypoint.y,
                dirtyScopeName(request.dirtyScope), request.reason);
            return;
        }
    }
    DepthBoneDirtyRequest request;
    request.root = root; request.parameter = parameter; request.keypoint = keypoint; request.targetUuid = targetUuid;
    request.dirtyScope = scope; request.reason = reason; request.actionSink = depthBoneRefreshActionSink;
    request.settleBeforeDispatch = settle;
    request.actionToken = depthBoneRefreshActionSink
        ? compat::ngRequireObject(depthBoneRefreshActionSink)->asyncToken() : actions::AsyncActionToken{};
    depthBoneDirtyRequests.append(std::move(request));
    if (depthBoneRefreshActionSink) compat::ngRequireObject(depthBoneRefreshActionSink)->markAsyncScheduled();
}
}
