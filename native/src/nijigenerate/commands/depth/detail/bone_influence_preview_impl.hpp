#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the production bone.cpp intake.
#include "bone_influence_preview.hpp"
#include "bone_input.hpp"
#include "bone_vector_math.hpp"
#include <nijigenerate/compat/throwable.hpp>

namespace nijigenerate::commands::depth::bone::detail {

nicxlive::core::math::Vec2Array generateInfluencePreviewOffsets(ext::nodes::ExDepthRigBinding* binding,
        std::shared_ptr<ext::nodes::ExDepthBone> bone,
        std::shared_ptr<nicxlive::core::nodes::Deformable> target) {
    if (!binding) failInput("Depth rig binding is required", 1327);
    if (!bone) failInput("Depth bone is required", 1328);
    if (!target) failInput("target is not deformable", 1329);
    nicxlive::core::math::Vec2Array offsets;
    offsets.setLength(target->getVertices().size());
    auto radius = segmentLength(bone->restHead, bone->restTail) * binding->influenceRule.radiusScale;
    if (radius < binding->influenceRule.minimumRadius) radius = binding->influenceRule.minimumRadius;
    if (radius <= 1e-6f) radius = 1.0f;
    const auto count = target->getVertices().size();
    for (std::size_t i = 0; i < count; ++i)
        offsets.set(i, nicxlive::core::math::Vec2{0.0f, radius * 0.2f});
    return offsets;
}

} // namespace nijigenerate::commands::depth::bone::detail
