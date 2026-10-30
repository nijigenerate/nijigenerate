#pragma once

#include "vertex.hpp"
#include <common/shared_array.hpp>
#include <cstdint>
#include <utility>

namespace nijigenerate::core::math::vertex {

template<class T>
::nicxlive::core::common::SharedArray<std::uint64_t> getInRect(
    ::nicxlive::core::common::SharedArray<T> vertices,
    ::nicxlive::core::math::Vec2 min, ::nicxlive::core::math::Vec2 max,
    std::uint32_t groupId = 0) {
    if (min.x > max.x) std::swap(min.x, max.x);
    if (min.y > max.y) std::swap(min.y, max.y);

    ::nicxlive::core::common::SharedArray<std::uint64_t> matching;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        auto vertex = vertices[index];
        if (min.x > position(vertex).x) continue;
        if (min.y > position(vertex).y) continue;
        if (max.x < position(vertex).x) continue;
        if (max.y < position(vertex).y) continue;
        if (groupId > 0 && !groupIdEquals(vertex, groupId)) continue;
        matching.push_back(static_cast<std::uint64_t>(index));
    }
    return matching;
}

} // namespace nijigenerate::core::math::vertex
