#pragma once

#include <math/types.hpp>

namespace nijigenerate::core::math::triangle::detail::grid_deformation {

inline nicxlive::core::math::Vec2 bilinear(nicxlive::core::math::Vec2 p00,
    nicxlive::core::math::Vec2 p10, nicxlive::core::math::Vec2 p01,
    nicxlive::core::math::Vec2 p11, float u, float v) {
    // Actual inmath vec2 multiplication/addition operates on each float component.
    const nicxlive::core::math::Vec2 a{
        p00.x * (1 - u) + p10.x * u, p00.y * (1 - u) + p10.y * u};
    const nicxlive::core::math::Vec2 b{
        p01.x * (1 - u) + p11.x * u, p01.y * (1 - u) + p11.y * u};
    return {a.x * (1 - v) + b.x * v, a.y * (1 - v) + b.y * v};
}

}
