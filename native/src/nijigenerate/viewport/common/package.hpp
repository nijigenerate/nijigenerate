#pragma once

#include <math/types.hpp>
#include <math/veca.hpp>
#include <cstdint>

namespace nijigenerate::viewport::common {

nicxlive::core::math::Vec3Array ngCreateCircleBuffer(nicxlive::core::math::Vec2 origin,
    nicxlive::core::math::Vec2 radii, std::uint32_t segments);
nicxlive::core::math::Vec3Array ngCreateRectBuffer(nicxlive::core::math::Vec2 from,
    nicxlive::core::math::Vec2 to);
nicxlive::core::math::Vec3Array ngCreateLineBuffer(nicxlive::core::math::Vec2 from,
    nicxlive::core::math::Vec2 to);

} // namespace nijigenerate::viewport::common
