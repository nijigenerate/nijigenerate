#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the original bone.d production translation unit.
#include "bone_action_scope.hpp"
#include "bone_dirty_providers.hpp"
#include "bone_dirty_state.hpp"
#include "bone_startup_hooks.hpp"

namespace nijigenerate::commands::depth::bone::detail {

std::shared_ptr<actions::AsyncGroupAction> claimDepthBoneAsyncGroupAction(actions::ActionPtr action) {
    std::size_t unowned = 0;
    for (auto countedRequests = depthBoneDirtyRequests; auto request : countedRequests) {
        if (!request.actionSink) ++unowned;
    }
    if (unowned == 0) return nullptr;

    const auto owner = sourceDepthBoneObject<actions::AsyncGroupAction>(
        actions::ActionSlice<actions::ActionPtr>{action});
    owner->setLifecycleHandlers(&cancelDepthBoneRefreshForAction,
        &beginDepthBoneRefreshUndo, &endDepthBoneRefreshUndo,
        &beginDepthBoneRefreshRedo, &endDepthBoneRefreshRedo);
    owner->setMergeHandler(&mergeDepthBoneAsyncActions);
    for (auto claimedRequests = depthBoneDirtyRequests; auto& request : claimedRequests) {
        if (request.actionSink) continue;
        request.actionSink = owner;
        request.actionToken = owner->asyncToken();
        setDepthBoneUpdateOwner(request.root, request.targetUuid, owner->asyncToken());
    }
    owner->markAsyncScheduled(unowned);
    return owner;
}

} // namespace nijigenerate::commands::depth::bone::detail
