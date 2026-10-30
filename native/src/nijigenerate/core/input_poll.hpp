#pragma once

#include "nijigenerate/core/input_io_state.hpp"

namespace nijigenerate::core::input {

inline void ngInputPoll() {
    detail::io = &ImGui::GetIO();
}

} // namespace nijigenerate::core::input
