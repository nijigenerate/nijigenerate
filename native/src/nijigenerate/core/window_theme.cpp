#include "nijigenerate/core/window_theme_internal.hpp"
#include <imgui_internal.h>
#include <algorithm>
#include <iterator>
namespace nijigenerate::core::detail {
WindowThemeState& windowThemeState() {
    thread_local WindowThemeState state;
    return state;
}
}
namespace nijigenerate::core {
ImVec4 ngUiAccentColor(float alpha) { return ImVec4(.36f, .45f, .35f, alpha); }
bool ngGetDarkMode() { return detail::windowThemeState().isDarkMode; }
void ngPushDarkColorScheme() {
    auto* context = ImGui::GetCurrentContext();
    const auto& colors = detail::windowThemeState().darkColors;
    std::copy(colors.begin(), colors.end(), std::begin(context->Style.Colors));
}
void ngPushLightColorScheme() {
    auto* context = ImGui::GetCurrentContext();
    const auto& colors = detail::windowThemeState().lightColors;
    std::copy(colors.begin(), colors.end(), std::begin(context->Style.Colors));
}
void ngPopColorScheme() {
    auto* context = ImGui::GetCurrentContext();
    const auto& state = detail::windowThemeState();
    const auto& colors = state.isDarkMode ? state.darkColors : state.lightColors;
    std::copy(colors.begin(), colors.end(), std::begin(context->Style.Colors));
}
}
