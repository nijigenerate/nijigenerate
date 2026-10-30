#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/label_internal.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::widgets {
namespace {
// The same body consumes the real retained descriptor or the explicit native
// borrowed view. Nested source calls retain that actual transport type.
template<class Text> const char* endOf(const Text& text) {
    return text.data() ? text.data() + text.size() : text.data();
}

template<class Text> void drawText(Text text) { ImGui::TextUnformatted(text.data(), endOf(text)); }

template<class Text> void drawTextColored(ImVec4 color, Text text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.data(), endOf(text));
    ImGui::PopStyleColor();
}

template<class Text> void drawTextDisabled(Text text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextUnformatted(text.data(), endOf(text));
    ImGui::PopStyleColor();
}

template<class Text> void drawTextShadowed(Text text) {
    const auto origin = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(origin.x + 1, origin.y + 1));
    ngTextColored(ImVec4(.25f, .25f, .25f, .5f), text);
    ImGui::SetCursorPos(origin);
    ngText(text);
}

template<class Text> void drawTextBordered(Text text, ImVec4 borderColor) {
    const auto origin = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(origin.x + 1, origin.y)); ngTextColored(borderColor, text);
    ImGui::SetCursorPos(ImVec2(origin.x - 1, origin.y)); ngTextColored(borderColor, text);
    ImGui::SetCursorPos(ImVec2(origin.x, origin.y + 1)); ngTextColored(borderColor, text);
    ImGui::SetCursorPos(ImVec2(origin.x, origin.y - 1)); ngTextColored(borderColor, text);
    ImGui::SetCursorPos(origin);
    ngText(text);
}

template<class Text> void drawTextWrapped(Text text) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text.data(), endOf(text));
    ImGui::PopTextWrapPos();
}

template<class Text> void drawTextLabel(Text text) {
    auto* draw = ImGui::GetWindowDrawList();
    auto& style = ImGui::GetStyle();
    const auto origin = ImGui::GetCursorScreenPos();
    const auto measured = ngMeasureString(text);
    const float xPadding = style.FramePadding.x;
    const float yPadding = style.FramePadding.y;
    const ImVec2 topLeft(origin.x, origin.y - yPadding);
    const ImVec2 bottomRight(origin.x + measured.x + (xPadding * 2), origin.y + measured.y + yPadding);
    const auto outlineColor = ImGui::GetColorU32(ImGuiCol_Text, .15f);
    compat::ngRequireObject(draw)->AddRectFilled(
        ImVec2(topLeft.x - 1, topLeft.y - 1), ImVec2(bottomRight.x + 1, bottomRight.y + 1), outlineColor, 4);
    draw->AddRectFilled(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_WindowBg), 4);
    draw->AddText(ImVec2(origin.x + xPadding, origin.y), ImGui::GetColorU32(ImGuiCol_Text), text.data(), endOf(text));
}
} // namespace

void ngText(compat::RetainedStringSlice text) { drawText(text); }
void ngText(std::string_view text) { drawText(text); }
void ngTextColored(ImVec4 color, compat::RetainedStringSlice text) { drawTextColored(color, text); }
void ngTextColored(ImVec4 color, std::string_view text) { drawTextColored(color, text); }
void ngTextDisabled(compat::RetainedStringSlice text) { drawTextDisabled(text); }
void ngTextDisabled(std::string_view text) { drawTextDisabled(text); }
void ngTextShadowed(compat::RetainedStringSlice text) { drawTextShadowed(text); }
void ngTextShadowed(std::string_view text) { drawTextShadowed(text); }
void ngTextBordered(compat::RetainedStringSlice text, ImVec4 color) { drawTextBordered(text, color); }
void ngTextBordered(std::string_view text, ImVec4 color) { drawTextBordered(text, color); }
void ngTextWrapped(compat::RetainedStringSlice text) { drawTextWrapped(text); }
void ngTextWrapped(std::string_view text) { drawTextWrapped(text); }
void ngTextLabel(compat::RetainedStringSlice text) { drawTextLabel(text); }
void ngTextLabel(std::string_view text) { drawTextLabel(text); }

namespace detail {
void addUnderline(ImVec4 color) {
    auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    min.y = max.y;
    auto* draw = ImGui::GetWindowDrawList();
    const auto packed = ImGui::GetColorU32(color);
    compat::ngRequireObject(draw)->AddLine(min, max, packed, 1.0f);
}
}
}
