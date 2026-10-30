#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/label_internal.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui_internal.h>

namespace nijigenerate::widgets {
namespace {
// Keep both actual parameters alive throughout the icon and link callbacks.
template<class Text> bool drawTextLinkWithIcon(Text icon, Text text) {
    ngText(icon);
    ImGui::SameLine(0, 4);
    return ngTextLink(text);
}

template<class Text> bool drawTextLink(Text text, ImVec4 hoverColor, ImVec4 clickedColor, ImVec4 baseColor) {
    auto* window = ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return false;
    const auto cursor = ImGui::GetCursorPos();
    bool clicked = false, hovered = false, held = false;
    ImGui::NewLine();
    const auto* begin = text.data();
    const auto* end = begin ? begin + text.size() : begin;
    ImGui::PushID(begin, end);
    auto* storage = ImGui::GetStateStorage();
    constexpr std::string_view key = "LinkClicked";
    const auto clickedId = ImGui::GetID(key.data(), key.data() + key.size());
    const bool previouslyClicked = compat::ngRequireObject(storage)->GetBool(clickedId, false);
    const auto size = ngMeasureString(text);
    // The pinned core retains this obsolete API; preserve the source call timing.
    ImGui::SetItemAllowOverlap();
    const auto id = ImGui::GetID(begin, end);
    const auto position = window->DC.CursorPos;
    const ImRect bounds(position, ImVec2(position.x + size.x, position.y + size.y));
    ImGui::ItemSize(bounds);
    // D binding AllowItemOverlap and this pinned core's AllowOverlap are both4096.
    clicked = ImGui::ButtonBehavior(bounds, id, &hovered, &held, ImGuiButtonFlags_AllowOverlap);
    if (!ImGui::ItemAdd(bounds, id)) {
        ImGui::PopID();
        return false;
    }
    if (clicked) storage->SetBool(ImGui::GetID(key.data(), key.data() + key.size()), true);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::SetCursorPos(cursor);
    if (!held && hovered) {
        ngTextColored(hoverColor, text); detail::addUnderline(hoverColor);
    } else if ((held && hovered) || previouslyClicked) {
        ngTextColored(clickedColor, text); detail::addUnderline(clickedColor);
    } else {
        ngTextColored(baseColor, text); detail::addUnderline(baseColor);
    }
    ImGui::PopID();
    return clicked;
}
} // namespace
bool ngTextLinkWithIcon(compat::RetainedStringSlice icon, compat::RetainedStringSlice text) {
    return drawTextLinkWithIcon(icon, text);
}
bool ngTextLinkWithIcon(std::string_view icon, std::string_view text) { return drawTextLinkWithIcon(icon, text); }
bool ngTextLink(compat::RetainedStringSlice text, ImVec4 hover, ImVec4 clicked, ImVec4 base) {
    return drawTextLink(text, hover, clicked, base);
}
bool ngTextLink(std::string_view text, ImVec4 hover, ImVec4 clicked, ImVec4 base) {
    return drawTextLink(text, hover, clicked, base);
}
}
