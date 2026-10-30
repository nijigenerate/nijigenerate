#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the production bone.cpp intake.
#include "bone_grid_stopper.hpp"
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::commands::depth::bone::detail {

bool gridPropagationStopper(std::shared_ptr<nicxlive::core::nodes::GridDeformer> ancestor,
        std::shared_ptr<nicxlive::core::nodes::Node> target,
        std::shared_ptr<nicxlive::core::nodes::Node>& stopper) {
    stopper.reset();
    if (!ancestor || !target || ancestor == target) return false;
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> intermediates;
    auto cursor = target->parentPtr();
    while (cursor && cursor != ancestor) {
        intermediates.append(cursor);
        cursor = cursor->parentPtr();
    }
    if (cursor != ancestor) return false;
    for (auto remaining = intermediates.size(); remaining != 0; --remaining) {
        const auto intermediate = intermediates[remaining - 1];
        // Nested Grids retain residual Bone offsets, so outer Grid influence continues.
        if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(intermediate)) continue;
        if (!intermediate->mustPropagate()) {
            stopper = intermediate;
            break;
        }
    }
    return true;
}

} // namespace nijigenerate::commands::depth::bone::detail
