#pragma once

#include <math/types.hpp>
#include "delaunay_diagnostics.hpp"

namespace nijigenerate::core::math::triangle::detail::triangulation {

inline float sign(nicxlive::core::math::Vec2 p1, nicxlive::core::math::Vec2 p2,
    nicxlive::core::math::Vec2 p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

#if defined(NIJIGENERATE_DEBUG_DELAUNAY) && NIJIGENERATE_DEBUG_DELAUNAY
inline constexpr bool defaultDelaunayDiagnostics = true;
#else
inline constexpr bool defaultDelaunayDiagnostics = false;
#endif

template<bool DelaunayDiagnostics = defaultDelaunayDiagnostics>
inline bool inCircle(nicxlive::core::math::Vec2 pa, nicxlive::core::math::Vec2 pb,
    nicxlive::core::math::Vec2 pc, nicxlive::core::math::Vec2 pd) {
    if constexpr (DelaunayDiagnostics) diagnostics::begin(pa, pb, pc, pd);
    float adx = pa.x - pd.x;
    float ady = pa.y - pd.y;
    float bdx = pb.x - pd.x;
    float bdy = pb.y - pd.y;

    float adxbdy = adx * bdy;
    float bdxady = bdx * ady;
    float oabd = adxbdy - bdxady;

    if (oabd <= 0) return false;

    float cdx = pc.x - pd.x;
    float cdy = pc.y - pd.y;

    float cdxady = cdx * ady;
    float adxcdy = adx * cdy;
    float ocad = cdxady - adxcdy;

    if (ocad <= 0) return false;

    float bdxcdy = bdx * cdy;
    float cdxbdy = cdx * bdy;

    float alift = adx * adx + ady * ady;
    float blift = bdx * bdx + bdy * bdy;
    float clift = cdx * cdx + cdy * cdy;

    float det = alift * (bdxcdy - cdxbdy) + blift * ocad + clift * oabd;

    if constexpr (DelaunayDiagnostics) diagnostics::determinant(det);
    return det > 0;
}

}
