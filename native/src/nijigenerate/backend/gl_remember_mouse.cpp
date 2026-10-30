#include "nijigenerate/backend/gl_input_internal.hpp"
#include "nijigenerate/backend/gl_input_state.hpp"
#include <SDL3/SDL.h>
#include <bit>

namespace nijigenerate::backend::gl::detail {
namespace {

bool desktopOrigin(std::uint32_t windowID, int& x, int& y) {
    if (!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) return false;
    if (auto* window = SDL_GetWindowFromID(windowID)) {
        // SDL3 reports failure explicitly. Do not read undefined coordinates.
        return SDL_GetWindowPosition(window, &x, &y);
    }
    return false;
}

std::int32_t addSourceInteger(std::int32_t value, int offset) {
    static_assert(sizeof(int) == sizeof(std::int32_t));
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)
        + static_cast<std::uint32_t>(offset));
}

void remember(ImVec2 position) {
    auto& state = inputState();
    state.eventMousePosition = position;
    state.mousePositionEventThisFrame = true;
}

} // namespace

void rememberEventMousePosition(std::uint32_t windowID, std::int32_t x, std::int32_t y) {
    int windowX = 0, windowY = 0;
    if (desktopOrigin(windowID, windowX, windowY)) {
        x = addSourceInteger(x, windowX);
        y = addSourceInteger(y, windowY);
    }
    // Preserve integer addition before the source's float conversion.
    remember(scaleMousePosition(x, y));
}

void rememberSDLEventMousePosition(std::uint32_t windowID, float x, float y) {
    int windowX = 0, windowY = 0;
    if (desktopOrigin(windowID, windowX, windowY)) {
        x += static_cast<float>(windowX);
        y += static_cast<float>(windowY);
    }
    // SDL3 event coordinates are floats; do not truncate fractional input.
    remember(scaleEventMousePosition(x, y));
}

} // namespace nijigenerate::backend::gl::detail
