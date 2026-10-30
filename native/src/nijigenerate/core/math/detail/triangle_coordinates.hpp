#pragma once

#include <math/veca.hpp>
#include "nijigenerate/compat/slice.hpp"
#include <cstdint>

namespace nijigenerate::core::math::triangle::detail::resampling {

inline nicxlive::core::math::Vec2 transformPointInTriangleCoords(
    nicxlive::core::math::Vec2 pt, nicxlive::core::math::Vec2 offset,
    nicxlive::core::math::Vec2Array vertices, compat::Slice<std::int32_t>& triangle) {
    (void)pt;
    // The source value descriptor selects the actual mutable opIndex provider.
    const auto read = [&vertices](std::int32_t index) -> nicxlive::core::math::Vec2 {
        return vertices[static_cast<std::size_t>(index)];
    };
    const auto p1 = read(triangle[0]);
    const auto p2 = read(triangle[1]);
    const auto p3 = read(triangle[2]);
    const nicxlive::core::math::Vec2 axis0{p2.x - p1.x, p2.y - p1.y};
    const nicxlive::core::math::Vec2 axis1{p3.x - p1.x, p3.y - p1.y};
    return {(p1.x + axis0.x * offset.x) + axis1.x * offset.y,
            (p1.y + axis0.y * offset.x) + axis1.y * offset.y};
}

}
