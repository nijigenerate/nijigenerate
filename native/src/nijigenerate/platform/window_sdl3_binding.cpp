#include "nijigenerate/platform/window_sdl3_binding.hpp"
#include "nijigenerate/platform/generated/window_sdl3_binding.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <SDL3/SDL_version.h>

static_assert(SDL_VERSION_ATLEAST(3, 2, 0), "The window source requires selected SDL3.2 headers");

namespace nijigenerate::platform {

SDL3Binding ngSelectedSDL3Binding() noexcept {
    return {detail::selectedSDL3TargetFile, detail::selectedSDL3TargetType};
}

void ngRequireSDL3Runtime() {
    const int version = SDL_GetVersion();
    if (SDL_VERSIONNUM_MAJOR(version) != 3 || version < SDL_VERSIONNUM(3, 2, 0)) {
        throw compat::ThrowableException(compat::RetainedString("SDL3 runtime version 3.2 or newer is required"));
    }
}

} // namespace nijigenerate::platform
