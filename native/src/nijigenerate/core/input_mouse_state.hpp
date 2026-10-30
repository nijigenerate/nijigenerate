#pragma once

#include <math/types.hpp>
#include <limits>

namespace nijigenerate::core::input::detail {

inline thread_local ::nicxlive::core::math::Vec2 mpos{
    std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};

} // namespace nijigenerate::core::input::detail
