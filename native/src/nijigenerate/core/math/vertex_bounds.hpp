#pragma once

#include "vertex.hpp"
#include "vertex_proxy.hpp"
#include <common/shared_array.hpp>
#include <limits>

namespace nijigenerate::core::math::vertex {

template<class T>
void getBounds(::nicxlive::core::common::SharedArray<T> vertices,
               ::nicxlive::core::math::Vec2& min, ::nicxlive::core::math::Vec2& max) {
    constexpr float infinity = std::numeric_limits<float>::infinity();
    min = {infinity, infinity};
    max = {-infinity, -infinity};
    for (auto vertex : vertices) {
        if (min.x > position(vertex).x) min.x = position(vertex).x;
        if (min.y > position(vertex).y) min.y = position(vertex).y;
        if (max.x < position(vertex).x) max.x = position(vertex).x;
        if (max.y < position(vertex).y) max.y = position(vertex).y;
    }
}

} // namespace nijigenerate::core::math::vertex
