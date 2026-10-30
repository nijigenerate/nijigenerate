#include "detail/bone_pivot_cache.hpp"
#include "detail/bone_action_scope.hpp"
#include "detail/bone_pivot_observer.hpp"
#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_math.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/project.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::commands::depth::bone {
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;

namespace detail {

thread_local std::shared_ptr<ExDepthRigRoot> depthBoneEffectivePivotCacheRoot;
thread_local std::shared_ptr<ExDepthBone> depthBoneEffectivePivotCacheBone;
thread_local compat::Slice<DepthBoneSourceEffectivePivot> depthBoneEffectivePivotCache;
thread_local bool depthBoneEffectivePivotCacheDirty = false;
thread_local bool depthBoneEffectivePivotCacheReady = false;
thread_local std::uint64_t depthBoneEffectivePivotCacheGeneration = 0;
thread_local std::shared_ptr<PivotDependencyMap> depthBoneEffectivePivotDependencyUuids;
thread_local std::shared_ptr<nicxlive::core::Puppet> depthBoneEffectivePivotObservedPuppet;

void clearDepthBoneEffectivePivotSelection() {
    depthBoneEffectivePivotCacheRoot = nullptr;
    depthBoneEffectivePivotCacheBone = nullptr;
    depthBoneEffectivePivotCache.resize(0);
    if (depthBoneEffectivePivotDependencyUuids) depthBoneEffectivePivotDependencyUuids->clear();
    depthBoneEffectivePivotCacheDirty = false;
    depthBoneEffectivePivotCacheReady = false;
}

void syncDepthBoneEffectivePivotObserver() {
    auto puppet = project::incActivePuppet();
    if (puppet == depthBoneEffectivePivotObservedPuppet) return;

    using nicxlive::core::nodes::NodeNotifyListener;
    if (depthBoneEffectivePivotObserver && depthBoneEffectivePivotObservedPuppet &&
            depthBoneEffectivePivotObservedPuppet->root) {
        const auto receiver = depthBoneEffectivePivotObservedPuppet->root;
        const auto listener = NodeNotifyListener::fromMethod<&DepthBoneEffectivePivotObserver::onChange>(
            depthBoneEffectivePivotObserver);
        receiver->removeNotifyListener(listener);
    }

    clearDepthBoneEffectivePivotSelection();
    depthBoneEffectivePivotObservedPuppet = puppet;
    if (!depthBoneEffectivePivotObserver)
        depthBoneEffectivePivotObserver = sourceDepthBoneObject<DepthBoneEffectivePivotObserver>();
    if (puppet && puppet->root) {
        const auto receiver = puppet->root;
        const auto listener = NodeNotifyListener::fromMethod<&DepthBoneEffectivePivotObserver::onChange>(
            depthBoneEffectivePivotObserver);
        receiver->addNotifyListener(listener);
    }
}

void addDepthBoneEffectivePivotDependency(std::shared_ptr<nicxlive::core::nodes::Node> node) {
    for (auto cursor = node; cursor; cursor = cursor->parentRef()) {
        // Keep this actual parent alive during UUID callback re-entry without
        // substituting it for the later live traversal read.
        [[maybe_unused]] const auto parentLifetime = cursor->parentRef();
        const auto uuid = cursor->getUUID();
        if (!depthBoneEffectivePivotDependencyUuids)
            depthBoneEffectivePivotDependencyUuids = std::allocate_shared<PivotDependencyMap>(
                nicxlive::core::d_runtime::SourceAllocator<PivotDependencyMap>{});
        (*depthBoneEffectivePivotDependencyUuids)[uuid] = true;
    }
}

void rebuildDepthBoneEffectivePivotDependencies() {
    if (depthBoneEffectivePivotDependencyUuids) depthBoneEffectivePivotDependencyUuids->clear();
    auto puppet = project::incActivePuppet();
    if (!depthBoneEffectivePivotCacheRoot || !puppet || !puppet->root) return;
    [[maybe_unused]] const auto modelRootLifetime = puppet->root;
    addDepthBoneEffectivePivotDependency(depthBoneEffectivePivotCacheRoot);
    auto bindings = compat::ngRequireObject(depthBoneEffectivePivotCacheRoot)->bindings;
    for (auto& binding : bindings)
        addDepthBoneEffectivePivotDependency(findNodeByUuid(puppet->root, binding.targetUuid));
}

bool hasVisibleDepthBoneEffectivePivotSource(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<ExDepthBone> bone) {
    if (!root || !bone) return false;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        bool present = false;
        const auto sourceUuids = binding.sourceBoneUuids;
        const auto boneUuid = bone->getUUID();
        for (auto uuid : sourceUuids) {
            if (uuid == boneUuid) { present = true; break; }
        }
        if (!present) continue;
        if (ext::nodes::ngNormalizeDepthBoneSourceRotation(binding.sourceSetting(bone->getUUID()).rotation) != 0.0f)
            return true;
    }
    return false;
}

} // namespace detail

void ngSetDepthBoneEffectivePivotSelection(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<ExDepthBone> bone) {
    using namespace detail;
    syncDepthBoneEffectivePivotObserver();
    if (!root || !bone) {
        clearDepthBoneEffectivePivotSelection();
        return;
    }
    if (root == depthBoneEffectivePivotCacheRoot && bone == depthBoneEffectivePivotCacheBone) return;
    depthBoneEffectivePivotCacheRoot = root;
    depthBoneEffectivePivotCacheBone = bone;
    depthBoneEffectivePivotCache.resize(0);
    if (depthBoneEffectivePivotDependencyUuids) depthBoneEffectivePivotDependencyUuids->clear();
    depthBoneEffectivePivotCacheDirty = true;
    depthBoneEffectivePivotCacheReady = false;
}

void ngInvalidateDepthBoneEffectivePivotCache(std::shared_ptr<ExDepthRigRoot> root) {
    if (!detail::depthBoneEffectivePivotCacheRoot) return;
    if (!root || root == detail::depthBoneEffectivePivotCacheRoot)
        detail::depthBoneEffectivePivotCacheDirty = true;
}

void ngFlushDepthBoneEffectivePivotDirty() {
    using namespace detail;
    syncDepthBoneEffectivePivotObserver();
    if (!depthBoneEffectivePivotCacheDirty) return;
    depthBoneEffectivePivotCacheDirty = false;
    depthBoneEffectivePivotCacheReady = false;
    depthBoneEffectivePivotCache.resize(0);
    auto root = depthBoneEffectivePivotCacheRoot;
    auto bone = depthBoneEffectivePivotCacheBone;
    if (!root || !bone || !isLiveDepthRigRoot(root) || !rootContainsBone(root, bone)) {
        clearDepthBoneEffectivePivotSelection();
        return;
    }
    rebuildDepthBoneEffectivePivotDependencies();
    if (hasVisibleDepthBoneEffectivePivotSource(root, bone))
        depthBoneEffectivePivotCache = ngDepthBoneSourceEffectivePivots(root, bone);
    depthBoneEffectivePivotCacheReady = true;
    ++detail::depthBoneEffectivePivotCacheGeneration;
}

compat::Slice<DepthBoneSourceEffectivePivot> ngCachedDepthBoneSourceEffectivePivots(
        std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<ExDepthBone> bone) {
    using namespace detail;
    syncDepthBoneEffectivePivotObserver();
    if (!depthBoneEffectivePivotCacheReady || root != depthBoneEffectivePivotCacheRoot
            || bone != depthBoneEffectivePivotCacheBone) return {};
    return depthBoneEffectivePivotCache;
}

bool ngHasPendingDepthBoneEffectivePivotRefresh() {
    return detail::depthBoneEffectivePivotCacheDirty;
}

std::uint64_t ngDepthBoneEffectivePivotCacheGeneration() {
    return detail::depthBoneEffectivePivotCacheGeneration;
}

} // namespace nijigenerate::commands::depth::bone
