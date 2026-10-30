#pragma once

#include <imgui.h>
#include <cstdint>
#include <limits>

namespace nijigenerate::backend::gl::detail {

// Input history belongs to the source module's thread, not an ImGui context.
struct GLInputState {
    ImVec2 eventMousePosition{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    bool mousePositionEventThisFrame = false;
    std::uint32_t eventMouseButtons = 0;
};

inline GLInputState& inputState() {
    static thread_local GLInputState state;
    return state;
}

} // namespace nijigenerate::backend::gl::detail
