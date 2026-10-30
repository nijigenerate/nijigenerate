#include "nijigenerate/core/window.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/backend/runtime_gl_backend.hpp"
#include "nijigenerate/core/window_state_internal.hpp"
#include "nijigenerate/core/window_diagnostic_internal.hpp"
#include "nijigenerate/core/settings.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/core/logo.hpp"
#include "nijigenerate/platform/environment.hpp"
#include "nijigenerate/platform/window_sdl3_binding.hpp"
#include "nijigenerate/core/window_win32_dpi.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include "nijigenerate/ver.hpp"
#include <timing.hpp>

#include <SDL3/SDL.h>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::core {
namespace host = ::nijigenerate::platform;

void ngOpenWindow() {
    auto& state = detail::windowState();
    const auto desktop = host::ngGetEnvironmentVariable("XDG_SESSION_DESKTOP");
    constexpr std::array<std::string_view, 20> tilingDesktops{
        "i3", "awesome", "bspwm", "dwm", "echinus", "euclid-wm", "herbstluftwm", "leftwm",
        "notion", "qtile", "ratpoison", "snapwm", "stumpwm", "subtle", "wingo", "wmfs",
        "xmonad", "wayfire", "river", "labwc"};
    state.isTilingWM = false;
    if (desktop) {
        for (const auto name : tilingDesktops) {
            if (std::string_view(*desktop) == name) {
                state.isTilingWM = true;
                break;
            }
        }
    }

    host::ngRequireSDL3Runtime();
    // SDL3 requires this native-IME-UI hint before initialization.
    (void)SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "none");
    constexpr SDL_InitFlags subsystems = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC |
        SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_SENSOR;
    if (!SDL_Init(subsystems)) {
        throw compat::ThrowableException(detail::retainWindowText("Error initializing SDL3! ", SDL_GetError()),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/window.d"), 221);
    }
    (void)SDL_EnableScreenSaver();
    ngSetWin32DPIAwareness();

    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    (void)SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    (void)SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    (void)SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    (void)SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    (void)SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    (void)SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    (void)SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (settings::incSettingsGet<bool>(settings::SettingsString(
            compat::RetainedString::fromStaticLiteral("WinMax")), false)) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }
    if (!settings::incSettingsGet<bool>(settings::SettingsString(
            compat::RetainedString::fromStaticLiteral("DisableCompositor")))) {
        (void)SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    }

#if defined(NIJIGENERATE_WINDOW_ORDINARY_DEBUG)
    const auto suffix = translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("(Debug Mode)"));
#else
    const auto suffix = compat::RetainedString::fromStaticView(ver::INC_VERSION);
#endif
    const compat::RetainedStringSlice retainedTitle{detail::retainWindowText("nijigenerate ", suffix.view())};
    state.window = detail::tryCreateWindow(retainedTitle, flags);
    if (!state.window) throw compat::ThrowableException(detail::retainWindowText("Failed to create SDL window!"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/window.d"), 259);
    (void)SDL_ShowWindow(state.window);

    state.gl_context = SDL_GL_CreateContext(state.window);
    if (!state.gl_context) {
        SDL_DestroyWindow(state.window);
        (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        state.window = detail::tryCreateWindow(retainedTitle, flags);
        state.gl_context = SDL_GL_CreateContext(state.window);
    }
    if (!state.gl_context) {
        throw compat::ThrowableException(detail::retainWindowText("Failed to create GL 3.2 or 3.1 core context!"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/window.d"), 280);
    }
    (void)SDL_GL_SetSwapInterval(1);
    (void)host::ngGLBackendContextVersion();

    // Select the actual application backend before inInit initializes it.
    (void)backend::runtime_gl::ngSelectOpenGLRenderBackend();
    nicxlive::core::inInit([] { return ImGui::GetTime(); });
    ngCreateContext();
    ngInitLogo();
    const settings::SettingsString statsKey(compat::RetainedString::fromStaticLiteral("NerdStats"));
    ngShowStatsForNerds = settings::incSettingsCanGet(statsKey)
        ? settings::incSettingsGet<bool>(statsKey) : false;
}

} // namespace nijigenerate::core
