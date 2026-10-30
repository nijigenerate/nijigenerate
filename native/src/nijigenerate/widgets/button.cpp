#include <nijigenerate/widgets/button.hpp>
#include <cmath>

namespace nijigenerate::widgets {

const ImVec4 colorUndefined(
    std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());

namespace {
thread_local ImVec4 buttonTextColor = colorUndefined;
}

ImVec4 ngButtonTextColor() { return buttonTextColor; }
void ngButtonTextColor(ImVec4 color) { buttonTextColor = color; }

bool ngButtonColored(const char* text, ImVec2 size, ImVec4 textColor) {
    auto& style = ImGui::GetStyle();
    const ImVec2 originalPadding = style.FramePadding;
    if (size.x != 0) style.FramePadding.x = 0;
    if (size.y != 0) style.FramePadding.y = 0;
    const auto finite = [](ImVec4 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
               std::isfinite(value.z) && std::isfinite(value.w);
    };
    bool pressed = false;
    if (!finite(textColor)) {
        if (finite(buttonTextColor)) {
            ImGui::PushStyleColor(ImGuiCol_Text, buttonTextColor);
            pressed = ImGui::Button(text, size);
            ImGui::PopStyleColor();
        } else {
            pressed = ImGui::Button(text, size);
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        pressed = ImGui::Button(text, size);
        ImGui::PopStyleColor();
    }
    style.FramePadding = originalPadding;
    return pressed;
}

} // namespace nijigenerate::widgets
