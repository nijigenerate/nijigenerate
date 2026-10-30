#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include "nijigenerate/core/dpi.hpp"

namespace nijigenerate::backend::gl {

bool ngGLBackendInit(const char* glslVersion);
void ngGLBackendShutdown();
void ngGLBackendNewFrame();
void ngGLBackendPlatformNewFrame();
void ngGLBackendBeginRender();
bool ngGLBackendProcessEvent(const SDL_Event* event);

// Source module-level static retains public visibility; it is not C++ internal linkage.
void ngGLBackendSetupRenderState(ImDrawData* drawData, float framebufferWidth,
    float framebufferHeight, GLuint vertexArrayObject);
void ngGLBackendRenderDrawData(ImDrawData* drawData);
bool ngGLBackendCreateFontsTexture();
void ngGLBackendDestroyFontsTexture();
bool ngGLBackendCheckShader(GLuint handle, const char* description);
bool ngGLBackendCheckProgram(GLuint handle, const char* description);
bool ngGLBackendCreateDeviceObjects();
void ngGLBackendDestroyDeviceObjects();

#if defined(NG_NO_UI_SCALING)
extern "C" void ngGLBackendPlatformRenderWindow(ImGuiViewport* viewport, void* renderArgument);
#endif
void ngGLBackendPlatformInterfaceInit();
void ngGLBackendPlatformInterfaceShutdown();

} // namespace nijigenerate::backend::gl
