#include <nijigenerate/widgets/button.hpp>

namespace nijigenerate::widgets {
void ngEndDropdownMenu() {
    ImGui::EndPopup();
    ImGui::PopID();
}
} // namespace nijigenerate::widgets
