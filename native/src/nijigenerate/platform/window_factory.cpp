#include "nijigenerate/platform/window_factory.hpp"
#include <limits>

namespace nijigenerate::platform {

SDL_Window* ngCreateSDL3Window(const char* title, std::int32_t width, std::int32_t height,
    SDL_WindowFlags flags) {
    static_assert(std::numeric_limits<int>::digits == 31);
    auto* window = SDL_CreateWindow(title, static_cast<int>(width), static_cast<int>(height), flags);
    if (window) SDL_SetWindowMinimumSize(window, 960, 720);
    return window;
}

} // namespace nijigenerate::platform
