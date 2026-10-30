#pragma once

#include "../bone_effective_pivots.hpp"
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <puppet.hpp>
#include <common/resource_key_map.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace nijigenerate::commands::depth::bone {

namespace detail {

struct PivotDependencyHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        constexpr auto shift = sizeof(std::size_t) < sizeof(value) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
using PivotDependencyMap = nicxlive::core::common::ResourceKeyMap<std::uint64_t, bool,
    PivotDependencyHash, std::equal_to<std::uint64_t>>;
extern thread_local std::shared_ptr<ext::nodes::ExDepthRigRoot> depthBoneEffectivePivotCacheRoot;
extern thread_local std::shared_ptr<ext::nodes::ExDepthBone> depthBoneEffectivePivotCacheBone;
extern thread_local compat::Slice<DepthBoneSourceEffectivePivot> depthBoneEffectivePivotCache;
extern thread_local bool depthBoneEffectivePivotCacheDirty;
extern thread_local bool depthBoneEffectivePivotCacheReady;
extern thread_local std::uint64_t depthBoneEffectivePivotCacheGeneration;
extern thread_local std::shared_ptr<PivotDependencyMap> depthBoneEffectivePivotDependencyUuids;
extern thread_local std::shared_ptr<nicxlive::core::Puppet> depthBoneEffectivePivotObservedPuppet;

void clearDepthBoneEffectivePivotSelection();
void addDepthBoneEffectivePivotDependency(std::shared_ptr<nicxlive::core::nodes::Node> node);
void rebuildDepthBoneEffectivePivotDependencies();
bool hasVisibleDepthBoneEffectivePivotSource(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone);

// Uses the real NodeNotifyListener bound-method identity for remove/add.
void syncDepthBoneEffectivePivotObserver();

} // namespace detail
} // namespace nijigenerate::commands::depth::bone
