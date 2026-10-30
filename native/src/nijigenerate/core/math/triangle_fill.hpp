#pragma once
#include "detail/triangle_fill_scalar.hpp"
#include "triangle_resampling.hpp"
#include "vertex.hpp"
#include "nijigenerate/compat/vec3u.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <math/triangle.hpp>

namespace nijigenerate::core::math::triangle {
template<class Pixel, class Vertices, detail::FillScalarIndex Index, class Value>
void fillPoly(compat::Slice<Pixel> texture, std::uint64_t width, std::uint64_t height,
    ::nicxlive::core::math::Vec4 bounds, Vertices vertices,
    compat::Slice<Index> indices, std::uint64_t index, Value value) {
    if (vertices.size() < 3) return;
    const auto read = [&](std::uint64_t offset, std::size_t line) {
        if (offset >= indices.size()) throw compat::ngStaticError<compat::ArrayIndexError>(offset, indices.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), line);
        const auto vertexIndex = static_cast<std::uint64_t>(indices[offset]);
        if (vertexIndex >= vertices.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            vertexIndex, vertices.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), line);
        auto vertex = vertices[vertexIndex];
        return ::nijigenerate::core::math::vertex::position(vertex);
    };
    std::array<::nicxlive::core::math::Vec2, 3> positions{
        read(3 * index, 735), read(3 * index + 1, 736), read(3 * index + 2, 737)};
    const std::array<std::uint32_t, 3> triangleIndices{0, 1, 2};
    fillPoly(texture, width, height, bounds, std::span<const ::nicxlive::core::math::Vec2>(positions),
        std::span<const std::uint32_t>(triangleIndices), 0, value);
}
template<class Pixel, class Vertices, class Value>
void fillPoly(compat::Slice<Pixel> texture, std::uint64_t width, std::uint64_t height,
    ::nicxlive::core::math::Vec4 bounds, Vertices vertices,
    compat::Slice<compat::Vec3u> indices, std::uint64_t index, Value value) {
    using ::nicxlive::core::math::Vec2;
    using ::nicxlive::core::math::Vec2Array;
    if (vertices.size() < 3 || indices.size() < index) return;
    if (index >= indices.size()) return;
    const auto tri = indices[index];
    if (tri.x >= vertices.size() || tri.y >= vertices.size() || tri.z >= vertices.size()) return;
    const auto read = [&](std::size_t i) {
        auto vertex = vertices[i];
        return ::nijigenerate::core::math::vertex::position(vertex);
    };
    std::array<Vec2, 3> points{read(tri.x), read(tri.y), read(tri.z)};
    const auto tbounds = getBounds(points);
    const auto bwidth = compat::ngFloatToInt32(std::ceil(tbounds.z) - std::floor(tbounds.x) + 1.0f);
    const auto bheight = compat::ngFloatToInt32(std::ceil(tbounds.w) - std::floor(tbounds.y) + 1.0f);
    const auto top = compat::ngFloatToInt32(std::floor(tbounds.y));
    const auto left = compat::ngFloatToInt32(std::floor(tbounds.x));
    // Phobos min(int, ulong) returns int and compares signedness safely.
    const auto minimumDimension = [](std::int32_t dimension, std::uint64_t limit) {
        return dimension < 0 || static_cast<std::uint64_t>(dimension) <= limit ? dimension :
            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(limit));
    };
    const float ymin = 0.0f < bounds.y ? bounds.y : 0.0f;
    const float ymax = static_cast<float>(minimumDimension(bheight, height));
    for (float y = ymin; y < ymax; ++y) {
        const float xmin = 0.0f < bounds.x ? bounds.x : 0.0f;
        const float xmax = static_cast<float>(minimumDimension(bwidth, width));
        for (float x = xmin; x < xmax; ++x) {
            Vec2 point{static_cast<float>(left) + x, static_cast<float>(top) + y};
            if (::nicxlive::core::math::isPointInTriangle(point, Vec2Array{points[0], points[1], points[2]})) {
                point.x -= bounds.x; point.y -= bounds.y;
                const auto flat = compat::ngFloatToInt32(point.y * static_cast<float>(width) + point.x);
                if (static_cast<std::uint64_t>(flat) < texture.size()) texture[static_cast<std::size_t>(flat)] = value;
            }
        }
    }
}

template<class Point, class Polygon>
bool pointInPolygon(Point point, Polygon polygon, std::uint32_t groupId) {
#ifndef NDEBUG
    if (polygon.size() % 2 != 0)
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"),822);
#endif
    // Use the same position/group providers as the original module's UFCS calls.
    const auto positionOf = [](auto value) {
        if constexpr (requires { vertex::position(value); }) return vertex::position(value);
        else return ::nicxlive::core::math::Vec2{value.x, value.y}; // Polygon vertices use .xy in D.
    };
    const auto groupMatches = [&] {
        if constexpr (requires { vertex::groupIdEquals(point, groupId); })
            return vertex::groupIdEquals(point, groupId);
        else
            // SoA views represent the original vec2 value, which has no group restriction.
            return vertex::groupIdEquals(positionOf(point), groupId);
    };
    if (groupId > 0 && !groupMatches()) return false;
    std::ptrdiff_t crossings = 0;
    for (std::size_t i = 0; i < polygon.size(); i += 2) {
        const auto p1 = positionOf(polygon[i]);
        if (i + 1 >= polygon.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i + 1,polygon.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"),829);
        const auto p2 = positionOf(polygon[i + 1]);
        const auto p = positionOf(point);
        auto lower = p1, upper = p2;
        if (lower.y > upper.y) std::swap(lower, upper);
        if (lower.y <= p.y && p.y <= upper.y && lower.y != upper.y) {
            const float crossX = p1.x + (p.y - p1.y) / (p2.y - p1.y) * (p2.x - p1.x);
            if (p.x < crossX) { if (p1.y < p2.y) ++crossings; else --crossings; }
        }
    }
    return crossings != 0;
}
} // namespace nijigenerate::core::math::triangle
