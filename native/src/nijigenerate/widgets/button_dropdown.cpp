#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace nijigenerate::widgets {
namespace {
bool finiteTextColor() {
    const auto c = ngButtonTextColor();
    return std::isfinite(c.x) && std::isfinite(c.y) && std::isfinite(c.z) && std::isfinite(c.w);
}
}

bool ngDropdownButtonRetained(compat::RetainedString id, ImVec2 size, bool open) {
    return ngDropdownButton(id.view(), size, open);
}

bool ngDropdownButton(std::string_view id, ImVec2 size, bool open) {
    auto* context = ImGui::GetCurrentContext();
    auto* window = ImGui::GetCurrentWindow();
    auto& style = ImGui::GetStyle();
    if (compat::ngRequireObject(window)->SkipItems) return false;
    ImGui::SameLine(0, 0);
    const float frameHeight = ImGui::GetFrameHeight();
    if (size.x <= 0) size.x = 16;
    if (size.y <= 0) size.y = frameHeight;
    const auto* begin = id.data();
    const auto* end = begin ? begin + id.size() : begin;
    const auto itemId = ImGui::GetID(begin, end);
    const auto position = window->DC.CursorPos;
    const ImRect bounds(position, ImVec2(position.x + size.x, position.y + size.y));
    ImGui::ItemSize(size, size.y >= frameHeight
        ? compat::ngRequireObject(context)->Style.FramePadding.y : -1.0f);
    if (!ImGui::ItemAdd(bounds, itemId)) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bounds, itemId, &hovered, &held, ImGuiButtonFlags_None);
    const auto color = ImGui::GetColorU32(style.Colors[((held && hovered) || open) ? ImGuiCol_ButtonActive :
        hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button]);
    ImGui::RenderNavHighlight(bounds, itemId);
    const auto rounding = compat::ngRequireObject(context)->Style.FrameRounding;
    ImGui::RenderFrame(bounds.Min, bounds.Max, color, true, rounding);
    const std::string_view arrow = "\xee\x8c\x93";
    const auto arrowSize = ngMeasureString(arrow);
    if (finiteTextColor()) ImGui::PushStyleColor(ImGuiCol_Text, ngButtonTextColor());
    ImGui::RenderText(ImVec2(bounds.Min.x + std::max(0.0f, (size.x - arrowSize.x) * .5f),
                             bounds.Min.y + std::max(0.0f, (size.y - arrowSize.y) * .5f)),
        arrow.data(), arrow.data() + arrow.size(), true);
    if (finiteTextColor()) ImGui::PopStyleColor();
    return pressed;
}

bool ngDropdownButtonIconRetained(compat::RetainedString id, compat::RetainedString icon, ImVec2 size, bool open) {
    return ngDropdownButtonIcon(id.view(), icon.view(), size, open);
}

bool ngDropdownButtonIcon(std::string_view id, std::string_view icon, ImVec2 size, bool open) {
    if (icon.empty()) return ngDropdownButton(id, size, open);
    auto* context = ImGui::GetCurrentContext();
    auto* window = ImGui::GetCurrentWindow();
    auto& style = ImGui::GetStyle();
    if (compat::ngRequireObject(window)->SkipItems) return false;
    ImGui::SameLine(0, 0);
    const auto iconSize = ngMeasureString(icon);
    const float frameHeight = ImGui::GetFrameHeight();
    if (size.x <= 0) size.x = iconSize.x + 28;
    if (size.y <= 0) size.y = frameHeight;
    const auto* begin = id.data();
    const auto* end = begin ? begin + id.size() : begin;
    const auto itemId = ImGui::GetID(begin, end);
    const auto position = window->DC.CursorPos;
    const ImRect bounds(position, ImVec2(position.x + size.x, position.y + size.y));
    ImGui::ItemSize(size, size.y >= frameHeight
        ? compat::ngRequireObject(context)->Style.FramePadding.y : -1.0f);
    if (!ImGui::ItemAdd(bounds, itemId)) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bounds, itemId, &hovered, &held, ImGuiButtonFlags_None);
    const auto color = ImGui::GetColorU32(style.Colors[((held && hovered) || open) ? ImGuiCol_ButtonActive :
        hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button]);
    ImGui::RenderNavHighlight(bounds, itemId);
    const auto rounding = compat::ngRequireObject(context)->Style.FrameRounding;
    ImGui::RenderFrame(bounds.Min, bounds.Max, color, true, rounding);
    const std::string_view arrow = "\xee\x8c\x93";
    const auto arrowSize = ngMeasureString(arrow);
    if (finiteTextColor()) ImGui::PushStyleColor(ImGuiCol_Text, ngButtonTextColor());
    const auto* iconBegin = icon.data();
    const auto* iconEnd = iconBegin ? iconBegin + icon.size() : iconBegin;
    ImGui::RenderText(ImVec2(bounds.Min.x + 4.0f,
                             bounds.Min.y + std::max(0.0f, (size.y - iconSize.y) * .5f)),
        iconBegin, iconEnd, true);
    ImGui::RenderText(ImVec2(bounds.Min.x + std::max(0.0f, size.x - (arrowSize.x + 2)),
                             bounds.Min.y + std::max(0.0f, (size.y - arrowSize.y) * .5f)),
        arrow.data(), arrow.data() + arrow.size(), true);
    if (finiteTextColor()) ImGui::PopStyleColor();
    return pressed;
}
} // namespace nijigenerate::widgets
