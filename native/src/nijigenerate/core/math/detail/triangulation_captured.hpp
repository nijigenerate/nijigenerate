#pragma once

#include "triangulation_predicates.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/vec3u.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include <math/veca.hpp>

namespace nijigenerate::core::math::triangle::detail::triangulation {

inline bool contains(compat::Vec3u tri, nicxlive::core::math::Vec2 pt,
    nicxlive::core::math::Vec2Array& vtx) {
    const auto read = [&vtx](std::uint32_t index) {
        return static_cast<nicxlive::core::math::Vec2>(vtx[index]);
    };
    // Keep each captured-array read in original source order.
    auto a = read(tri.x);
    auto b = read(tri.y);
    const float d1 = sign(pt, a, b);
    a = read(tri.y);
    b = read(tri.z);
    const float d2 = sign(pt, a, b);
    a = read(tri.z);
    b = read(tri.x);
    const float d3 = sign(pt, a, b);
    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

inline void orientTri(std::uint32_t tri, std::uint32_t edge,
    compat::Slice<compat::Vec3u>& tris, compat::Slice<compat::Vec3u>& tri2edge) {
    const auto t2e = tri2edge[tri];
    const auto pt = tris[tri];
    if (t2e.x == edge) {
        return;
    } else if (t2e.y == edge) {
        tri2edge[tri] = {t2e.y, t2e.z, t2e.x};
        tris[tri] = {pt.y, pt.z, pt.x};
    } else if (t2e.z == edge) {
        tri2edge[tri] = {t2e.z, t2e.x, t2e.y};
        tris[tri] = {pt.z, pt.x, pt.y};
    } else {
        compat::ngAssertUnreachable("triangle does not own edge",
            "source/nijigenerate/core/math/triangle.d", 473);
    }
}

}
