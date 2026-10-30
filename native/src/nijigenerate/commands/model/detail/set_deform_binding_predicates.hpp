#pragma once

#include <math/veca.hpp>
#include "nijigenerate/compat/vec2u.hpp"

namespace nijigenerate::commands::model::set_deform_binding::detail {

inline bool ngAllOffsetsZero(nicxlive::core::math::Vec2Array& offsets) {
    return offsets.forEach([](nicxlive::core::math::Vec2View offset) -> int {
        if (offset.x != 0 || offset.y != 0) {
            return 1;
        }
        return 0;
    }) == 0;
}

inline bool ngSameKeyPoint(compat::Vec2u a, compat::Vec2u b) {
    return a.x == b.x && a.y == b.y;
}

}
