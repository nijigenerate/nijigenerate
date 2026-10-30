#include "nijigenerate/core/window_win32_dpi.hpp"

#include <Windows.h>
#include <shellscalingapi.h>
#include <SDL3/SDL_loadso.h>
#include <bit>
#include <cstdint>

namespace nijigenerate::platform::detail {
namespace {

template<class Function>
Function loadFunction(SDL_SharedObject* object, const char* name) {
    static_assert(sizeof(Function) == sizeof(SDL_FunctionPointer));
    return std::bit_cast<Function>(SDL_LoadFunction(object, name));
}

DPI_AWARENESS_CONTEXT sourceContext(SourceDPIAwarenessContext value) {
    return reinterpret_cast<DPI_AWARENESS_CONTEXT>(static_cast<std::intptr_t>(value));
}

} // namespace

void setWin32DPIAwareness() {
    SDL_SharedObject* userDLL = SDL_LoadObject("USER32.DLL");
    decltype(&::SetProcessDPIAware) dpiAware8 = nullptr;
    decltype(&::SetProcessDpiAwarenessContext) dpiAwareContext = nullptr;
    decltype(&::SetProcessDpiAwareness) dpiAware81 = nullptr;
    if (userDLL) {
        dpiAware8 = loadFunction<decltype(dpiAware8)>(userDLL, "SetProcessDPIAware");
        dpiAwareContext = loadFunction<decltype(dpiAwareContext)>(userDLL, "SetProcessDpiAwarenessContext");
    }

    SDL_SharedObject* shcoreDLL = SDL_LoadObject("SHCORE.DLL");
    if (shcoreDLL) {
        dpiAware81 = loadFunction<decltype(dpiAware81)>(shcoreDLL, "SetProcessDpiAwareness");
    }

    if (dpiAwareContext) {
        (void)dpiAwareContext(sourceContext(SourceDPIAwarenessContext::perMonitorAware));
        (void)dpiAwareContext(sourceContext(SourceDPIAwarenessContext::perMonitorAwareV2));
    } else if (dpiAware81) {
        (void)dpiAware81(static_cast<PROCESS_DPI_AWARENESS>(SourceProcessDPIAwareness::perMonitorAware));
    } else if (dpiAware8) {
        (void)dpiAware8();
    }

    if (userDLL) SDL_UnloadObject(userDLL);
    if (shcoreDLL) SDL_UnloadObject(shcoreDLL);
}

} // namespace nijigenerate::platform::detail

namespace nijigenerate::core {
void ngSetWin32DPIAwareness() {
    ::nijigenerate::platform::detail::setWin32DPIAwareness();
}
}
