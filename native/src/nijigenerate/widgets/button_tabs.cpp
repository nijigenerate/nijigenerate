#include <nijigenerate/widgets/button.hpp>
#include <cmath>

namespace nijigenerate::widgets {

bool ngBeginTabItem(const char* text, bool* open, ImGuiTabItemFlags flags) {
    const auto color = ngButtonTextColor();
    bool pressed = false;
    if (std::isfinite(color.x) && std::isfinite(color.y) && std::isfinite(color.z) && std::isfinite(color.w)) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        pressed = ImGui::BeginTabItem(text, open, flags);
        ImGui::PopStyleColor();
    } else {
        pressed = ImGui::BeginTabItem(text, open, flags);
    }
    return pressed;
}

void ngEndTabItem() { ImGui::EndTabItem(); }

} // namespace nijigenerate::widgets
