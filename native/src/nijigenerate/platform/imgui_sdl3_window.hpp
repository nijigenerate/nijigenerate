#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>

namespace nijigenerate::platform {

// Borrowed for the current SDL main-thread call. Context and viewport must be live.
SDL_Window* ngResolveImGuiSDL3Window(ImGuiContext* context, ImGuiViewport* viewport);

// The rectangle is already in this SDL window's coordinate system. This function
// deliberately does not infer a GUI screen/DPI coordinate conversion.
bool ngSetImGuiSDL3WindowTextInputArea(ImGuiContext* context, ImGuiViewport* viewport,
    const SDL_Rect& windowArea, int cursorOffset = 0);

// Preserve extents, translating an ImGui screen-coordinate rectangle into the
// registered viewport's SDL window coordinates, as the pinned SDL3 backend does.
bool ngSetImGuiSDL3ScreenTextInputArea(ImGuiContext* context, ImGuiViewport* viewport,
    const SDL_Rect& screenArea, int cursorOffset = 0);

// Project UI units use the actual configured platform transform, followed by
// subtraction of the registered viewport's SDL desktop origin.
bool ngSetImGuiSDL3UITextInputArea(ImGuiContext* context, ImGuiViewport* viewport,
    const SDL_Rect& uiArea, int cursorOffset = 0);

} // namespace nijigenerate::platform
