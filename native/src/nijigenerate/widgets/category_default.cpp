#include "nijigenerate/widgets/category.hpp"

namespace nijigenerate::widgets {
bool ngBeginCategory(const char* title, IncCategoryFlags flags, CategoryCallback callback) {
    const auto color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    return ngBeginCategory(title, color, flags, callback);
}
}
