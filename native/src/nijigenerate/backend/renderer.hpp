#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
namespace nijigenerate::backend {
void ngInitializeRenderer(SDL_Window*);
void ngInitializeRendererUI();
void ngShutdownRenderer();
void ngBeginRendererFrame();
void ngRenderUI(ImDrawData*);
bool ngRendererProcessEvent(const SDL_Event*);
void ngCreateRendererFonts();
void ngDestroyRendererFonts();
}
