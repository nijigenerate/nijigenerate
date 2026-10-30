#pragma once

#include "nijigenerate/core/input_mouse_state.hpp"

namespace nijigenerate::core::input {

inline ::nicxlive::core::math::Vec2 ngInputGetMousePosition() noexcept {
    return detail::mpos;
}

} // namespace nijigenerate::core::input
