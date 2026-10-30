#pragma once
#include <math/types.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
#include <ranges>

namespace nijigenerate::core::math::triangle {

template<class T>
::nicxlive::core::math::Vec4 getBounds(T& vertices) {
    constexpr float maximum = std::numeric_limits<float>::max();
    ::nicxlive::core::math::Vec4 bounds{maximum, maximum, -maximum, -maximum};
    const auto minimum = [](auto a, auto b) { return b < a ? b : a; };
    const auto maximumValue = [](auto a, auto b) { return a < b ? b : a; };
    const auto visit = [&](auto&& vertex) {
        bounds = {static_cast<float>(minimum(bounds.x, vertex.x)), static_cast<float>(minimum(bounds.y, vertex.y)),
            static_cast<float>(maximumValue(bounds.z, vertex.x)), static_cast<float>(maximumValue(bounds.w, vertex.y))};
    };
    if constexpr (std::ranges::range<T>) {
        for (auto&& vertex : vertices) visit(vertex);
    } else {
        // Native Vec2Array exposes its SoA proxy through indexed access.
        for (std::size_t i = 0; i < vertices.size(); ++i) visit(vertices[i]);
    }
    bounds.x = std::floor(bounds.x);
    bounds.y = std::floor(bounds.y);
    bounds.z = std::ceil(bounds.z);
    bounds.w = std::ceil(bounds.w);
    return bounds;
}

} // namespace nijigenerate::core::math::triangle

