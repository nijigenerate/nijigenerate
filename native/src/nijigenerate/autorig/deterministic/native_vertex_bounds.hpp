#pragma once

#include <nijigenerate/core/settings_value.hpp>
#include <math/veca.hpp>
#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace nijigenerate::autorig::deterministic::native_detail {

template<class Vertices>
core::settings::SettingsJsonValue reviewVertexBounds(Vertices vertices) {
    if (!vertices.size()) return {};
    constexpr auto infinity = std::numeric_limits<double>::infinity();
    std::array<double, 4> bounds{infinity, infinity, -infinity, -infinity};
    const auto consume = [&](auto point) {
        // min/max keep the first operand on ties and unordered comparisons.
        bounds[0] = std::min<double>(bounds[0], point.x);
        bounds[1] = std::min<double>(bounds[1], point.y);
        bounds[2] = std::max<double>(bounds[2], point.x);
        bounds[3] = std::max<double>(bounds[3], point.y);
    };
    if constexpr (std::is_same_v<Vertices, nicxlive::core::math::Vec2Array>) {
        // The genuine SoA provider exposes at(), not C++ begin/end iterators.
        // Read actual points directly, without allocating an AoS conversion.
        const auto count = vertices.size();
        for (std::size_t i = 0; i < count; ++i) consume(vertices.at(i));
    } else {
        for (auto point : vertices) consume(point);
    }
    core::settings::SettingsJsonArray result;
    for (const auto value : bounds) result.append(core::settings::SettingsJsonValue(value));
    return core::settings::SettingsJsonValue(std::move(result));
}

} // namespace nijigenerate::autorig::deterministic::native_detail
