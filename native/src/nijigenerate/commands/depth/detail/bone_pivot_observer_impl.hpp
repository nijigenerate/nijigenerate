#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the production bone.cpp intake. No eager receiver allocation.
#include "bone_pivot_observer.hpp"
#include "bone_pivot_cache.hpp"
#include "bone_dirty_providers.hpp"

namespace nijigenerate::commands::depth::bone::detail {

thread_local std::shared_ptr<DepthBoneEffectivePivotObserver> depthBoneEffectivePivotObserver;

void DepthBoneEffectivePivotObserver::onChange(std::shared_ptr<nicxlive::core::nodes::Node> target,
        nicxlive::core::nodes::NotifyReason) {
    if (!target || !depthBoneEffectivePivotCacheReady) return;
    const auto uuid = target->getUUID();
    const auto dependencies = depthBoneEffectivePivotDependencyUuids;
    if ((dependencies && dependencies->find(uuid) != dependencies->end()) ||
            isSameOrAncestorNode(depthBoneEffectivePivotCacheRoot, target)) {
        depthBoneEffectivePivotCacheDirty = true;
    }
}

} // namespace nijigenerate::commands::depth::bone::detail
