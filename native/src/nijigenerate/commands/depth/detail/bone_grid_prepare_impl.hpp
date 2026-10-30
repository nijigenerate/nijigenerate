#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the production bone.cpp intake.
#include "bone_grid_prepare.hpp"
#include "bone_grid_index.hpp"
#include <nijigenerate/compat/phobos_float_sort.hpp>

namespace nijigenerate::commands::depth::bone::detail {

bool prepareGridOffsetSampling(std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid,
        nicxlive::core::math::Vec2Array offsets, compat::Slice<float>& axisX, compat::Slice<float>& axisY) {
    axisX = compat::Slice<float>{};
    axisY = compat::Slice<float>{};
    if (!grid || offsets.size() != grid->getVertices().size()) return false;
    const auto vertices = grid->getVertices();
    const auto count = vertices.size();
    for (std::size_t i = 0; i < count; ++i) {
        appendUniqueGridAxis(axisX, vertices.xAt(i));
        appendUniqueGridAxis(axisY, vertices.yAt(i));
    }
    compat::phobos_float_sort::sort({axisX.data(), axisX.size()});
    compat::phobos_float_sort::sort({axisY.data(), axisY.size()});
    return axisX.size() >= 2 && axisY.size() >= 2 &&
        axisX.size() * axisY.size() == grid->getVertices().size();
}

} // namespace nijigenerate::commands::depth::bone::detail
