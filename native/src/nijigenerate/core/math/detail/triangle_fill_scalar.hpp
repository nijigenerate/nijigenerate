#pragma once
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/core/math/triangle_bounds.hpp"
#include "nijigenerate/core/math/vertex.hpp"
#include <math/triangle.hpp>
#include <math/veca.hpp>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace nijigenerate::core::math::triangle {
namespace detail {
template<class T> concept FillScalarIndex =
    std::same_as<T, std::int8_t> || std::same_as<T, std::uint8_t>
    || std::same_as<T, std::int16_t> || std::same_as<T, std::uint16_t>
    || std::same_as<T, std::int32_t> || std::same_as<T, std::uint32_t>
    || std::same_as<T, std::int64_t> || std::same_as<T, std::uint64_t>;

inline std::int32_t fillScalarAdd(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
}

// Preserve the source's independent texture-element and assigned-value types.
template<class Pixel, detail::FillScalarIndex Index, class Value>
void fillPoly(compat::Slice<Pixel> texture, std::uint64_t width, std::uint64_t height,
    ::nicxlive::core::math::Vec4 bounds,
    std::span<const ::nicxlive::core::math::Vec2> vertices,
    std::span<const Index> indices, std::uint64_t index, Value value) {
    using ::nicxlive::core::math::Vec2;
    using ::nicxlive::core::math::Vec2Array;
    (void)height;
    if (vertices.size() < 3) return;
    const auto read = [&](std::uint64_t offset, std::size_t line) {
        if (offset >= indices.size()) throw compat::ngStaticError<compat::ArrayIndexError>(offset, indices.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), line);
        const auto vertexIndex = static_cast<std::uint64_t>(indices[static_cast<std::size_t>(offset)]);
        if (vertexIndex >= vertices.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            vertexIndex, vertices.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), line);
        return vertex::position(vertices[static_cast<std::size_t>(vertexIndex)]);
    };
    std::array<Vec2, 3> triangle{read(3 * index, 735), read(3 * index + 1, 736), read(3 * index + 2, 737)};
    const auto tbounds = getBounds(triangle);
    const float widthValue = std::ceil(tbounds.z) - std::floor(tbounds.x) + 1.0f;
    const float heightValue = std::ceil(tbounds.w) - std::floor(tbounds.y) + 1.0f;
    const auto bwidth = compat::ngFloatToInt32(widthValue);
    const auto bheight = compat::ngFloatToInt32(heightValue);
    const auto top = compat::ngFloatToInt32(std::floor(tbounds.y));
    const auto left = compat::ngFloatToInt32(std::floor(tbounds.x));
    for (std::int32_t y = 0; y < bheight; ++y) {
        for (std::int32_t x = 0; x < bwidth; ++x) {
            Vec2 point{static_cast<float>(detail::fillScalarAdd(left, x)),
                static_cast<float>(detail::fillScalarAdd(top, y))};
            if (::nicxlive::core::math::isPointInTriangle(point,
                    Vec2Array{triangle[0], triangle[1], triangle[2]})) {
                point.x -= bounds.x;
                point.y -= bounds.y;
                const float product = point.y * static_cast<float>(width);
                const auto flat = compat::ngFloatToInt32(product + point.x);
                const auto textureIndex = static_cast<std::size_t>(flat);
                if (textureIndex >= texture.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                    textureIndex, texture.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), 751);
                // The source performs the same write on both sides of this guard.
                if (static_cast<std::uint64_t>(flat) >= texture.size())
                    texture[static_cast<std::size_t>(flat)] = value;
                else
                    texture[static_cast<std::size_t>(flat)] = value;
            }
        }
    }
}
} // namespace nijigenerate::core::math::triangle
