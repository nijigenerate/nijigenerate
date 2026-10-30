#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Original private-module bodies; included once by the production bone.cpp intake.
#include "bone_grid_index.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <cmath>

namespace nijigenerate::commands::depth::bone::detail {

bool isSameOrDescendantBone(std::shared_ptr<ext::nodes::ExDepthBone> bone,
        std::shared_ptr<ext::nodes::ExDepthBone> ancestor) {
    std::shared_ptr<nicxlive::core::nodes::Node> cursor = bone;
    while (cursor) {
        if (cursor == ancestor) return true;
        cursor = cursor->parentPtr();
    }
    return false;
}

std::size_t nodeHierarchyDepth(std::shared_ptr<nicxlive::core::nodes::Node> node) {
    std::size_t result = 0;
    for (auto cursor = node; cursor; cursor = cursor->parentPtr()) ++result;
    return result;
}

void appendUniqueGridAxis(compat::Slice<float>& axis, float value) {
    for (auto existingAxis = axis; const auto existing : existingAxis) {
        if (std::abs(existing - value) <= 0.0001f) return;
    }
    axis.append(value);
}

std::ptrdiff_t gridVertexIndex(const nicxlive::core::math::Vec2Array vertices, float x, float y) {
    const auto count = vertices.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (std::abs(vertices.xAt(i) - x) <= 0.0001f &&
            std::abs(vertices.yAt(i) - y) <= 0.0001f) return static_cast<std::ptrdiff_t>(i);
    }
    return -1;
}

std::size_t gridAxisInterval(compat::ConstSlice<float> axis, float value) {
    return gridAxisInterval(axis.view(), value);
}

std::size_t gridAxisInterval(std::span<const float> axis, float value) {
    if (axis.size() < 2) return 0;
    const auto intervalCount = axis.size() - 1;
    for (std::size_t i = 0; i < intervalCount; ++i) {
        if (value <= compat::ngArrayAt(axis, i + 1)) return i;
    }
    return axis.size() - 2;
}

} // namespace nijigenerate::commands::depth::bone::detail
