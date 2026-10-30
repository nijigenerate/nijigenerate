#pragma once

#include <nodes/drawable.hpp>
#include "nijigenerate/core/math/detail/triangle_coordinates.hpp"
#include "nijigenerate/core/math/triangle_sign.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <cmath>
#include <bit>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nijigenerate::core::math::triangle::detail::resampling {

using nicxlive::core::nodes::MeshData;
using nicxlive::core::nodes::Deformation;
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec2Array;

template<class... Args> inline std::shared_ptr<Deformation> allocateDeformation(Args&&... args) {
    try {
        return std::make_shared<Deformation>(std::forward<Args>(args)...);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<std::size_t N> inline compat::RetainedString enforcementMessage(const char (&message)[N]) {
    try {
        return compat::RetainedString(std::string_view(message, N - 1));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline Vec2 meshVertex(MeshData& mesh, std::int32_t index) {
    return mesh.vertices[static_cast<std::size_t>(index)];
}

inline bool usableTriangle(MeshData& mesh, compat::Slice<std::int32_t> triangle,
                           Deformation& deform) {
    // The source captures deformation rather than copying it for each predicate.
    for (const auto index : triangle) {
        if (index < 0 || static_cast<std::size_t>(index) >= mesh.vertices.size() ||
            static_cast<std::size_t>(index) >= deform.vertexOffsets.size()) return false;
        const auto point = meshVertex(mesh, index);
        const Vec2 offset = deform.vertexOffsets[static_cast<std::size_t>(index)];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(offset.x) || !std::isfinite(offset.y)) return false;
    }
    const auto b = meshVertex(mesh, triangle[1]);
    const auto a = meshVertex(mesh, triangle[0]);
    const Vec2 ab{b.x - a.x, b.y - a.y};
    const auto c = meshVertex(mesh, triangle[2]);
    const auto aAgain = meshVertex(mesh, triangle[0]);
    const Vec2 ac{c.x - aAgain.x, c.y - aAgain.y};
    return std::abs(static_cast<double>(ab.x) * ac.y - static_cast<double>(ab.y) * ac.x) > 1e-8;
}

inline bool isPointInTriangle(Vec2 pt, compat::Slice<std::int32_t> triangle, MeshData& bindingMesh) {
    auto p1 = meshVertex(bindingMesh, triangle[0]);
    auto p2 = meshVertex(bindingMesh, triangle[1]);
    auto p3 = meshVertex(bindingMesh, triangle[2]);
    const auto d1 = surrounding_triangle::sign(pt, p1, p2);
    const auto d2 = surrounding_triangle::sign(pt, p2, p3);
    const auto d3 = surrounding_triangle::sign(pt, p3, p1);
    const auto hasNeg = d1 < 0 || d2 < 0 || d3 < 0;
    const auto hasPos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(hasNeg && hasPos);
}

inline std::int32_t searchAdd(std::int32_t value, std::uint32_t increment) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + increment);
}

inline compat::Slice<std::int32_t> findSurroundingTriangle(Vec2 point, MeshData& mesh,
                                                         Deformation& deform) {
    std::int32_t i = 0;
    compat::Slice<std::int32_t> triangle{0, 1, 2};
    while (static_cast<std::size_t>(searchAdd(i, 2)) < mesh.indices.size()) {
        triangle[0] = mesh.indices[static_cast<std::size_t>(i)];
        triangle[1] = mesh.indices[static_cast<std::size_t>(searchAdd(i, 1))];
        triangle[2] = mesh.indices[static_cast<std::size_t>(searchAdd(i, 2))];
        if (usableTriangle(mesh, triangle, deform) && isPointInTriangle(point, triangle, mesh))
            return triangle;
        i = searchAdd(i, 3);
    }
    return {};
}

inline Vec2 calcOffsetInTriangleCoords(Vec2 pt, MeshData& bindingMesh,
                                       compat::Slice<std::int32_t>& triangle) {
    const auto p1 = meshVertex(bindingMesh, triangle[0]);
    const auto p2 = meshVertex(bindingMesh, triangle[1]);
    const auto p3 = meshVertex(bindingMesh, triangle[2]);
    const Vec2 ab{p2.x - p1.x, p2.y - p1.y};
    const Vec2 ac{p3.x - p1.x, p3.y - p1.y};
    const Vec2 delta{pt.x - p1.x, pt.y - p1.y};
    const double determinant = static_cast<double>(ab.x) * ac.y - static_cast<double>(ab.y) * ac.x;
    return {static_cast<float>((static_cast<double>(delta.x) * ac.y - static_cast<double>(delta.y) * ac.x) / determinant),
            static_cast<float>((static_cast<double>(ab.x) * delta.y - static_cast<double>(ab.y) * delta.x) / determinant)};
}

inline Vec2Array transformMesh(MeshData& bindingMesh, Deformation deform) {
    Vec2Array result;
    if (bindingMesh.vertices.size() != deform.vertexOffsets.size())
        throw compat::ThrowableException{enforcementMessage(
            "Mesh and deformation vertex counts differ during resampling"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), 176};
    for (std::size_t i = 0; i < bindingMesh.vertices.size(); ++i) {
        const Vec2 v = bindingMesh.vertices[i];
        const Vec2 offset = deform.vertexOffsets[i];
        result.push_back({v.x + offset.x, v.y + offset.y});
    }
    return result;
}

} // namespace nijigenerate::core::math::triangle::detail::resampling
