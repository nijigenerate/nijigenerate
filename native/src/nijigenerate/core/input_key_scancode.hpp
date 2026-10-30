#pragma once

#include <imgui.h>
#include <string_view>

namespace nijigenerate::core::input {

inline ImGuiKey ngKeyScancode(std::string_view code) noexcept {
    struct Mapping { std::string_view code; ImGuiKey key; };
    static constexpr Mapping mappings[] = {
        {"A", ImGuiKey_A}, {"B", ImGuiKey_B}, {"C", ImGuiKey_C}, {"D", ImGuiKey_D},
        {"E", ImGuiKey_E}, {"F", ImGuiKey_F}, {"G", ImGuiKey_G}, {"H", ImGuiKey_H},
        {"I", ImGuiKey_I}, {"J", ImGuiKey_J}, {"K", ImGuiKey_K}, {"L", ImGuiKey_L},
        {"M", ImGuiKey_M}, {"N", ImGuiKey_N}, {"O", ImGuiKey_O}, {"P", ImGuiKey_P},
        {"Q", ImGuiKey_Q}, {"R", ImGuiKey_R}, {"S", ImGuiKey_S}, {"T", ImGuiKey_T},
        {"U", ImGuiKey_U}, {"V", ImGuiKey_V}, {"W", ImGuiKey_W}, {"X", ImGuiKey_X},
        {"Y", ImGuiKey_Y}, {"Z", ImGuiKey_Z},
        {"0", ImGuiKey_0}, {"1", ImGuiKey_1}, {"2", ImGuiKey_2}, {"3", ImGuiKey_3},
        {"4", ImGuiKey_4}, {"5", ImGuiKey_5}, {"6", ImGuiKey_6}, {"7", ImGuiKey_7},
        {"8", ImGuiKey_8}, {"9", ImGuiKey_9},
        {"F1", ImGuiKey_F1}, {"F2", ImGuiKey_F2}, {"F3", ImGuiKey_F3}, {"F4", ImGuiKey_F4},
        {"F5", ImGuiKey_F5}, {"F6", ImGuiKey_F6}, {"F7", ImGuiKey_F7}, {"F8", ImGuiKey_F8},
        {"F9", ImGuiKey_F9}, {"F10", ImGuiKey_F10}, {"F11", ImGuiKey_F11}, {"F12", ImGuiKey_F12},
        {"Left", ImGuiKey_LeftArrow}, {"Right", ImGuiKey_RightArrow},
        {"Up", ImGuiKey_UpArrow}, {"Down", ImGuiKey_DownArrow}
    };
    for (const auto& entry : mappings) if (entry.code == code) return entry.key;
    return ImGuiKey_None;
}

} // namespace nijigenerate::core::input
