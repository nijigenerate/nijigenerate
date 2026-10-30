#pragma once
#include "bone_grid_prepare.hpp"
#include "bone_dirty_providers.hpp"
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/compat/const_slice.hpp>

namespace nijigenerate::commands::depth::bone::detail {
using DepthBoneResolvedOffsets = core::selector::SelectorMap<
    std::uint32_t, nicxlive::core::math::Vec2Array>;

bool sampleGridOffset(std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid,
    nicxlive::core::math::Vec2Array offsets, compat::ConstSlice<float> axisX, compat::ConstSlice<float> axisY,
    nicxlive::core::math::Vec2 point, nicxlive::core::math::Vec2& sampled);
bool addParentGridInfluence(std::shared_ptr<nicxlive::core::nodes::GridDeformer> parentGrid,
    std::shared_ptr<nicxlive::core::nodes::Deformable> target,
    nicxlive::core::math::Vec2Array parentOffsets, nicxlive::core::math::Vec2Array& accumulated);
bool addParentGridNodeOriginInfluence(std::shared_ptr<nicxlive::core::nodes::GridDeformer> parentGrid,
    std::shared_ptr<nicxlive::core::nodes::Node> stopper,
    std::shared_ptr<nicxlive::core::nodes::Deformable> target,
    nicxlive::core::math::Vec2Array parentOffsets, nicxlive::core::math::Vec2Array& accumulated);
nicxlive::core::math::Vec2Array depthBoneAncestorGridInfluence(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::nodes::Deformable> target,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    nicxlive::core::math::Vec2Array targetOffsets, DepthBoneResolvedOffsets& resolvedOffsets);
} // namespace nijigenerate::commands::depth::bone::detail
