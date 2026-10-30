// Altered port of the independent D Markdown helper. The complete original
// zlib copyright and license notice is preserved in markdown.hpp in this module.
#include "nijigenerate/widgets/markdown_internal.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"

namespace nijigenerate::widgets::markdown::detail {
void underline(ImVec4 color) {
    ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    minimum.y = maximum.y;
    auto* drawList = ImGui::GetWindowDrawList();
    const auto packedColor = ImGui::GetColorU32(color);
    compat::ngRequireObject(drawList)->AddLine(minimum, maximum, packedColor, 1.0f);
}
}
