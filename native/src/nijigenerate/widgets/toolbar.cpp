/*
    Copyright (c) 2020-2023, Inochi2D Project
    Copyright (c) 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Original author: Luna Nielsen
*/
#include "nijigenerate/widgets/toolbar.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/widgets/label.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <imgui_internal.h>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets {
bool ngBeginInnerToolbar(float height, bool matchTitlebar, bool offset) {
    auto& style = ImGui::GetStyle();
    auto* window = ImGui::GetCurrentWindow();
    const auto barColor = matchTitlebar ? (
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ?
            style.Colors[ImGuiCol_TitleBgActive] : style.Colors[ImGuiCol_TitleBg]) :
        style.Colors[ImGuiCol_MenuBarBg];
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, barColor);
    ImGui::PushStyleColor(ImGuiCol_Button, barColor);
    if (!compat::ngRequireObject(window)->IsExplicitChild) {
        // Preserve the source coordinate order and parent clip ownership.
        ImGui::PushClipRect(ImVec2(window->OuterRectClipped.Max.x,
            offset ? window->OuterRectClipped.Max.y - 1 : window->OuterRectClipped.Max.y),
            ImVec2(window->OuterRectClipped.Min.x, window->OuterRectClipped.Min.y), false);
        ImGui::SetCursorPosY(offset ? ImGui::GetCursorPosY() - 1 : ImGui::GetCursorPosY());
    }
    const bool visible = ImGui::BeginChild("###Toolbar", ImVec2(0, height), 0,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (visible) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + style.FramePadding.x);
    return visible;
}

void ngEndInnerToolbar() {
    auto* window = ImGui::GetCurrentWindow();
    if (!compat::ngRequireObject(window)->IsExplicitChild) ImGui::PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
}

bool ngToolbarButton(const char* text, float width) {
    const bool clicked = ImGui::Button(text, ImVec2(width, ngAvailableSpace().y));
    ImGui::SameLine(0, 0);
    return clicked;
}

void ngToolbarSpacer(float space) {
    ngDummy(ImVec2(space, 0));
    ImGui::SameLine(0, 0);
}

void ngToolbarSeparator() {
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(.5f, .5f, .5f, 1));
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 6);
    ImGui::PopStyleColor();
}

void ngToolbarText(compat::RetainedStringSlice text) {
    ImGui::SetCursorPosY(6);
    ngText(text);
    ImGui::SameLine(0, 4);
}
void ngToolbarText(std::string_view text) {
    compat::RetainedString retained;
    try {
        retained = compat::RetainedString(text);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    ngToolbarText(compat::RetainedStringSlice(retained));
}
} // namespace nijigenerate::widgets
