#include "nijigenerate/platform/imgui_sdl3_window.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/platform/imgui_sdl3_adapter.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

#if !SDL_VERSION_ATLEAST(3, 2, 0)
#error "The IME window provider requires the selected SDL 3.2 APIs"
#endif

namespace nijigenerate::platform {

SDL_Window* ngResolveImGuiSDL3Window(ImGuiContext* context, ImGuiViewport* viewport) {
    if (!SDL_IsMainThread()) throw std::logic_error("IME window resolution requires the SDL main thread");
    if (!context || ImGui::GetCurrentContext() != context) {
        throw std::logic_error("IME window resolution requires the actual current ImGui context");
    }
    const auto& io = ImGui::GetIO();
    if (!io.BackendPlatformUserData || !io.BackendPlatformName
        || (std::strcmp(io.BackendPlatformName, "imgui_impl_sdl3") != 0
            && std::strcmp(io.BackendPlatformName, "nijigenerate_imgui_sdl3_adapter") != 0)) {
        throw std::logic_error("IME window resolution requires the actual pinned ImGui SDL3 backend");
    }
    if (!viewport || ImGui::FindViewportByID(viewport->ID) != viewport) {
        throw std::logic_error("IME viewport is not registered in the current ImGui context");
    }
    // The pinned SDL3 backend stores SDL_WindowID in PlatformHandle, including
    // the main viewport. PlatformHandleRaw is an OS handle, not SDL_Window*.
    const auto encoded = reinterpret_cast<std::uintptr_t>(viewport->PlatformHandle);
    if (encoded == 0 || encoded > std::numeric_limits<SDL_WindowID>::max()) {
        throw std::logic_error("IME viewport has no valid SDL window ID");
    }
    auto* window = SDL_GetWindowFromID(static_cast<SDL_WindowID>(encoded));
    if (!window) throw std::runtime_error("IME viewport SDL window is unavailable");
    return window;
}

bool ngSetImGuiSDL3WindowTextInputArea(ImGuiContext* context, ImGuiViewport* viewport,
    const SDL_Rect& windowArea, int cursorOffset) {
    auto* window = ngResolveImGuiSDL3Window(context, viewport);
    return SDL_SetTextInputArea(window, &windowArea, cursorOffset);
}

bool ngSetImGuiSDL3ScreenTextInputArea(ImGuiContext* context, ImGuiViewport* viewport,
    const SDL_Rect& screenArea, int cursorOffset) {
    auto* window = ngResolveImGuiSDL3Window(context, viewport);
    // The pinned SDL3 backend uses logical window units for IME, not drawable
    // pixels: subtract the viewport origin and do not apply framebuffer scale.
    const SDL_Rect windowArea{
        compat::ngFloatToInt32(static_cast<float>(screenArea.x) - viewport->Pos.x),
        compat::ngFloatToInt32(static_cast<float>(screenArea.y) - viewport->Pos.y),
        screenArea.w, screenArea.h};
    return SDL_SetTextInputArea(window, &windowArea, cursorOffset);
}

bool ngSetImGuiSDL3UITextInputArea(ImGuiContext* context, ImGuiViewport* viewport,
    const SDL_Rect& uiArea, int cursorOffset) {
    auto* window = ngResolveImGuiSDL3Window(context, viewport);
    const float scale = ngImGuiSDL3CoordinateScale();
    const SDL_Rect windowArea{
        compat::ngFloatToInt32(static_cast<float>(uiArea.x) * scale - viewport->Pos.x),
        compat::ngFloatToInt32(static_cast<float>(uiArea.y) * scale - viewport->Pos.y),
        compat::ngFloatToInt32(static_cast<float>(uiArea.w) * scale),
        compat::ngFloatToInt32(static_cast<float>(uiArea.h) * scale)};
    return SDL_SetTextInputArea(window, &windowArea, cursorOffset);
}

} // namespace nijigenerate::platform
