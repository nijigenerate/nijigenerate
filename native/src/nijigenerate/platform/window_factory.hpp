#pragma once
#include <SDL3/SDL.h>
#include <cstdint>

namespace nijigenerate::platform {
SDL_Window* ngCreateSDL3Window(const char* title, std::int32_t width, std::int32_t height,
    SDL_WindowFlags flags);
} // namespace nijigenerate::platform
