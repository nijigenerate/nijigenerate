#pragma once

#include <SDL3/SDL_video.h>
#include <imgui.h>
#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/slice.hpp"

namespace nijigenerate::core::detail {

// Internal state for the single window source module. Handles remain borrowed.
// Theme and Wayland state retain their existing, separate storage owners.
struct WindowState {
    SDL_GLContext gl_context = nullptr;
    SDL_Window* window = nullptr;
    ImGuiIO* io = nullptr;
    bool done = false;
    ImGuiID viewportDock = 0;
    bool firstFrame = true;
    compat::RetainedStringSlice windowSubtitle{};
    bool lastWindowModified = false;
    compat::Slice<compat::RetainedStringSlice> files{};
    // Retains the elements behind ImGui's byte-copied, non-owning payload.
    compat::Slice<compat::RetainedStringSlice> fileDropPayloadOwner{};
    bool isTilingWM = false;
};

WindowState& windowState();
SDL_Window* tryCreateWindow(compat::RetainedStringSlice title, SDL_WindowFlags flags);

} // namespace nijigenerate::core::detail
