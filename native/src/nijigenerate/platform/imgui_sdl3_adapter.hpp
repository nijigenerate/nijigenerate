#pragma once
#include <SDL3/SDL_video.h>

namespace nijigenerate::platform {

using SDL3CoordinateScaleProvider = float (*)();

// Configure the actual current initialized project-owned platform backend.
// A manual transform requires the real application scale getter, not a fallback.
// The caller owns the SDL main thread/current ImGui context and callback lifetime.
// The getter is queried synchronously; its exceptions and invalid numeric values
// are not replaced with a guessed scale or a different backend's private data.
void ngConfigureImGuiSDL3Coordinates(bool manuallyScales, SDL3CoordinateScaleProvider provider);
float ngImGuiSDL3CoordinateScale();

// Read the live platform owner's real shared viewport contexts, without
// selecting a context or creating a parallel resource/context registry.
bool ngImGuiSDL3SharesOpenGLContext(SDL_GLContext owner, SDL_GLContext current);

} // namespace nijigenerate::platform
