#include "nijigenerate/core/window.hpp"
#include "nijigenerate/core/window_theme_internal.hpp"
#include "nijigenerate/core/settings.hpp"
#include <algorithm>
#include <iterator>

namespace nijigenerate::core {

void ngSetDarkMode(bool darkMode) {
    auto& style = ImGui::GetStyle();
    auto& state = detail::windowThemeState();
    const auto& colors = darkMode ? state.darkColors : state.lightColors;
    std::copy(colors.begin(), colors.end(), std::begin(style.Colors));

    settings::incSettingsSet(
        settings::SettingsString(compat::RetainedString::fromStaticLiteral("DarkMode")), darkMode);
    state.isDarkMode = darkMode;
}

} // namespace nijigenerate::core
