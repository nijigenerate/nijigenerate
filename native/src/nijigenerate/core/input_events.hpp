#pragma once

#include <imgui.h>
#include <cmath>

namespace nijigenerate::core::input {

inline bool ngInputIsMouseClicked(ImGuiMouseButton button) {
    return ImGui::IsMouseClicked(button, false);
}

inline bool ngInputIsMouseReleased(ImGuiMouseButton button) {
    return ImGui::IsMouseReleased(button);
}

inline bool ngInputIsDragRequested(ImGuiMouseButton button = ImGuiMouseButton_Right) {
    const auto delta = ImGui::GetMouseDragDelta(button, -1.0f);
    return std::abs(delta.x) > 0.1f || std::abs(delta.y) > 0.1f;
}

inline bool ngInputIsKeyDown(ImGuiKey key) {
    return ImGui::IsKeyDown(key);
}

inline bool ngInputIsKeyPressed(ImGuiKey key) {
    return ImGui::IsKeyPressed(key, true);
}

} // namespace nijigenerate::core::input
