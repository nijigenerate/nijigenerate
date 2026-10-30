#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nodes/deformable.hpp>
#include <math/veca.hpp>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

nicxlive::core::math::Vec2Array generateInfluencePreviewOffsets(ext::nodes::ExDepthRigBinding* binding,
    std::shared_ptr<ext::nodes::ExDepthBone> bone,
    std::shared_ptr<nicxlive::core::nodes::Deformable> target);

} // namespace nijigenerate::commands::depth::bone::detail
