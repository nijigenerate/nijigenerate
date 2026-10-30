#pragma once

#include <math/types.hpp>

namespace nijigenerate::core::math::triangle::detail::surrounding_triangle {
inline float sign(::nicxlive::core::math::Vec2& p1, ::nicxlive::core::math::Vec2& p2,
                  ::nicxlive::core::math::Vec2& p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}
} // namespace nijigenerate::core::math::triangle::detail::surrounding_triangle
