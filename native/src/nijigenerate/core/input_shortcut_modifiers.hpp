#pragma once

#include <nijigenerate/platform/shortcut_policy.hpp>
#include <imgui.h>
#include <utility>

namespace nijigenerate::core::input {

struct ShortcutModifiers {
    bool ctrl, alt, shift, superKey;
};

inline ShortcutModifiers ngInputShortcutModifiers(const ImGuiIO& io) {
    ShortcutModifiers result{io.KeyCtrl, io.KeyAlt, io.KeyShift, io.KeySuper};
    // ImGui 1.90.7+ normalizes physical Command to Ctrl on macOS. Our
    // persisted shortcuts and MacSwapCmdCtrl setting use physical key names.
    if constexpr (platform::shortcutAppleLabels) {
        if (io.ConfigMacOSXBehaviors) std::swap(result.ctrl, result.superKey);
    }
    return result;
}

} // namespace nijigenerate::core::input
