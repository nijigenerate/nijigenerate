#pragma once
#include <nijigenerate/compat/slice.hpp>
#include <math/types.hpp>
#include <memory>
#include <cstdint>
#include <limits>
namespace nijigenerate::ext::nodes { class ExDepthBone; class ExDepthRigRoot; }
namespace nijigenerate::commands::depth::bone {
struct DepthBoneSourceEffectivePivot {
    std::uint64_t targetUuid{};
    nicxlive::core::math::Vec3 bonePoint{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    nicxlive::core::math::Vec3 effectivePoint{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    float rotationPivotXShift = std::numeric_limits<float>::quiet_NaN();
    float rotation = std::numeric_limits<float>::quiet_NaN();
};
compat::Slice<DepthBoneSourceEffectivePivot> ngDepthBoneSourceEffectivePivots(
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root, std::shared_ptr<ext::nodes::ExDepthBone> bone);
void ngSetDepthBoneEffectivePivotSelection(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> bone);
void ngInvalidateDepthBoneEffectivePivotCache(std::shared_ptr<ext::nodes::ExDepthRigRoot> root = nullptr);
bool ngHasPendingDepthBoneEffectivePivotRefresh();
std::uint64_t ngDepthBoneEffectivePivotCacheGeneration();
void ngFlushDepthBoneEffectivePivotDirty();
compat::Slice<DepthBoneSourceEffectivePivot> ngCachedDepthBoneSourceEffectivePivots(
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root, std::shared_ptr<ext::nodes::ExDepthBone> bone);
}
