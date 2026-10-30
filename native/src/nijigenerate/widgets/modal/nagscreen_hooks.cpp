// Altered independent D Nagscreen port; the original BSD notice is in nagscreen.hpp.
#include "nijigenerate/widgets/modal/nagscreen.hpp"

namespace nijigenerate::widgets::modal {
void Nagscreen::onBeginUpdate() {
    ImGui::SetNextWindowSizeConstraints(uiSize,uiSize);
    origWindowPadding = ImGui::GetStyle().WindowPadding;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10.0f);
    Modal::onBeginUpdate();
}

void Nagscreen::onEndUpdate() {
    ImGui::PopStyleVar(2);
    Modal::onEndUpdate();
}
}
