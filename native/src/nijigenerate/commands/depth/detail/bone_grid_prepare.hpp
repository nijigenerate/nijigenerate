#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include <nijigenerate/compat/slice.hpp>
#include <nodes/grid_deformer.hpp>
#include <math/veca.hpp>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

bool prepareGridOffsetSampling(std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid,
    nicxlive::core::math::Vec2Array offsets, compat::Slice<float>& axisX, compat::Slice<float>& axisY);

} // namespace nijigenerate::commands::depth::bone::detail
