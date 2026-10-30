#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <math/veca.hpp>
#include <cstddef>
#include <memory>
#include <span>

namespace nijigenerate::commands::depth::bone::detail {

bool isSameOrDescendantBone(std::shared_ptr<ext::nodes::ExDepthBone> bone,
    std::shared_ptr<ext::nodes::ExDepthBone> ancestor);
std::size_t nodeHierarchyDepth(std::shared_ptr<nicxlive::core::nodes::Node> node);
void appendUniqueGridAxis(compat::Slice<float>& axis, float value);
std::ptrdiff_t gridVertexIndex(const nicxlive::core::math::Vec2Array vertices, float x, float y);
std::size_t gridAxisInterval(compat::ConstSlice<float> axis, float value);
std::size_t gridAxisInterval(std::span<const float> axis, float value);

} // namespace nijigenerate::commands::depth::bone::detail
