#include "nijigenerate/core/window.hpp"
#include "nijigenerate/compat/source_c_string.hpp"
#include "nijigenerate/compat/detail/source_c_string_contract.hpp"
#include "nijigenerate/core/window_state_internal.hpp"
#include "nijigenerate/core/path.hpp"
#include "nijigenerate/core/settings.hpp"
#include "nijigenerate/core/dpi.hpp"
#include "nijigenerate/core/font.hpp"
#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/widgets/dialog.hpp"
#include "nijigenerate/platform/window_context_policy.hpp"
#include "nijigenerate/platform/gl_backend_policy.hpp"
#include "nijigenerate/platform/imgui_sdl3_adapter.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <clocale>
#include <cstdlib>
#include <cstring>

namespace nijigenerate::core {

void ngCreateContext() {
    auto* context = ImGui::CreateContext(nullptr);
    (void)context;
    auto& state = detail::windowState();
    state.io = &ImGui::GetIO();
    auto& io = *state.io;

    const auto existsPath = path::ngGetAppImguiConfigFile();
    if (!::nijigenerate::platform::detail::windowConfigFileExists(existsPath.view())) {
        // The original base-config branch has no implementation.
    }

    io.IniFilename = static_cast<char*>(std::malloc(path::ngGetAppImguiConfigFile().size() + 1));
    // Use the selected Phobos provider's actual output-copy/contract policy;
    // the library contract configuration is independent of this TU's NDEBUG.
    const auto copyPath = path::ngGetAppImguiConfigFile();
    const compat::SourceCString copySource{compat::RetainedStringSlice(copyPath)};
    compat::detail::ngSourceCStringPostconditions(copyPath.view(), copySource.data());
    const auto copyLength = path::ngGetAppImguiConfigFile().size() + 1;
    std::memcpy(const_cast<char*>(io.IniFilename), copySource.data(), copyLength);
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    ngSetDarkMode(settings::incSettingsGet<bool>(
        settings::SettingsString(compat::RetainedString::fromStaticLiteral("DarkMode")), false));
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsResizeFromEdges = true;
    ::nijigenerate::platform::detail::configureWindowContextHost(io);

    // Exact literal-C branch of the independent i18nSetLocale implementation.
    (void)std::setlocale(LC_ALL, "C");
#if defined(NG_NO_UI_SCALING)
    if (!ngIsTilingWM()) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#else
    dpi::incInitDPIScaling();
#endif

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    (void)ImGui_ImplSDL3_InitForOpenGL(state.window, state.gl_context);
    ::nijigenerate::platform::ngConfigureImGuiSDL3Coordinates(
        ::nijigenerate::platform::ngGLBackendManuallyScalesViewport(), &dpi::incGetUIScale);
    (void)backend::gl::ngGLBackendInit(nullptr);
    font::incInitFonts();
    ngInitStyling();
    widgets::dialog::ngInitDialogs();
    ngResetClearColor();
}

} // namespace nijigenerate::core
