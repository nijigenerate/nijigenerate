#pragma once
#include "nijigenerate/core/window_theme.hpp"
#include <array>
#include <limits>
namespace nijigenerate::core::detail {
struct WindowThemeState {
    bool isDarkMode = false;
    std::array<ImVec4, ImGuiCol_COUNT> darkColors;
    std::array<ImVec4, ImGuiCol_COUNT> lightColors;
    WindowThemeState() {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        darkColors.fill(ImVec4(nan, nan, nan, nan));
        lightColors.fill(ImVec4(nan, nan, nan, nan));
    }
};
WindowThemeState& windowThemeState();
}
