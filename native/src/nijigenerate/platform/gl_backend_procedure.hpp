#pragma once
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <bit>
#include <cstdint>

namespace nijigenerate::platform {
// Other procedures are borrowed from the actual current desktop GL>=3 context.
// GenVertexArrays uses the Source loader's process-wide mutable binding.
struct GLGenVertexArraysBindingState {
    PFNGLGENVERTEXARRAYSPROC procedure{};
    bool resolved{};
};
GLGenVertexArraysBindingState ngGLGetGenVertexArraysBinding() noexcept;
void ngGLSetGenVertexArraysBinding(PFNGLGENVERTEXARRAYSPROC procedure) noexcept;
void ngGLRestoreGenVertexArraysBinding(GLGenVertexArraysBindingState state) noexcept;
SDL_FunctionPointer ngGLGetBackendProcedure(const char* name);
std::uint32_t ngGLBackendContextVersion();
template<class Function> Function ngGLBackendProcedure(const char* name) {
    static_assert(sizeof(Function) == sizeof(SDL_FunctionPointer));
    return std::bit_cast<Function>(ngGLGetBackendProcedure(name));
}
} // namespace nijigenerate::platform
