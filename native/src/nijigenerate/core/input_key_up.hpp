#pragma once

#include "nijigenerate/core/input_events.hpp"

namespace nijigenerate::core::input {

inline bool ngInputIsKeyUp(ImGuiKey key) {
    return !ngInputIsKeyDown(key);
}

} // namespace nijigenerate::core::input
