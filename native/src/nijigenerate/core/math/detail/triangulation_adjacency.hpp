#pragma once

#include "nijigenerate/compat/vec2u.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include <cstdint>

namespace nijigenerate::core::math::triangle::detail::triangulation {

inline void replaceE2T(compat::Vec2u& e2t, std::uint32_t from, std::uint32_t to) {
    if (e2t.x == from) {
        e2t.x = to;
        if (e2t.y == from) e2t.y = to;
    } else if (e2t.y == from) {
        e2t.y = to;
    } else {
        compat::ngAssertUnreachable("edge mismatch", "source/nijigenerate/core/math/triangle.d", 458);
    }
}

}
