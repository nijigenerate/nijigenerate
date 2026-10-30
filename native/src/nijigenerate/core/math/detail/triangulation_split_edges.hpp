#pragma once
#include "triangulation_captured.hpp"
#include "triangulation_adjacency.hpp"

namespace nijigenerate::core::math::triangle::detail::triangulation {
inline void splitEdges(compat::Slice<compat::Vec3u>& tris,
    compat::Slice<compat::Vec3u>& tri2edge, compat::Slice<compat::Vec2u>& edge2tri,
    ::nicxlive::core::math::Vec2Array& vertices) {
    // Capture the initial edge count; newly appended boundary edges wait for the next call.
    const auto edgeCount = static_cast<std::uint32_t>(edge2tri.size());
    for (std::uint32_t edge = 0; edge < edgeCount; ++edge) {
        const auto adjacency = edge2tri[edge];
        if (adjacency.x != adjacency.y) continue;
        orientTri(adjacency.x, edge, tris, tri2edge);
        const auto first = adjacency.x;
        const auto second = static_cast<std::uint32_t>(tris.size());
        const auto left = tris[first].x, right = tris[first].y, opposite = tris[first].z;
        const auto midpoint = static_cast<std::uint32_t>(vertices.size());
        const auto read = [&](std::uint32_t index) {
            return static_cast<::nicxlive::core::math::Vec2>(vertices[index]);
        };
        const auto a = read(left), b = read(right);
        vertices.push_back({(a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f});
        const auto outer = static_cast<std::uint32_t>(edge2tri.size());
        const auto internal = static_cast<std::uint32_t>(outer + std::uint32_t{1});
        const auto previous = tri2edge[first].y;
        tris[first].y = midpoint;
        tri2edge[first].y = internal;
        tris.append(compat::Vec3u{midpoint, right, opposite});
        tri2edge.append(compat::Vec3u{outer, previous, internal});
        edge2tri.append(compat::Vec2u{second, second});
        edge2tri.append(compat::Vec2u{first, second});
        replaceE2T(edge2tri[previous], first, second);
    }
}
} // namespace nijigenerate::core::math::triangle::detail::triangulation
