// Copyright 2020-2023 Inochi2D Project; 2024 nijigenerate Project.
// Author: Luna Nielsen. BSD-2-Clause; see LICENSE.
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <imgui_internal.h>
#include <cmath>
#include <new>
#include <stdexcept>

namespace nijigenerate::widgets {
namespace {
compat::RetainedStringSlice retainDummyText(std::string_view text) {
    try {
        return compat::RetainedStringSlice(compat::RetainedString(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>();
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>();
    }
}
}

void ngDummy(ImVec2 size) {
    const auto available = ngAvailableSpace();
    if (size.x <= 0) size.x = available.x - std::abs(size.x);
    if (size.y <= 0) size.y = available.y - std::abs(size.y);
    ImGui::Dummy(size);
}

void ngDummyLabel(compat::RetainedStringSlice text, ImVec2 area) {
    const auto available = ngAvailableSpace();
    auto size = area;
    if (size.x <= 0) size.x = available.x;
    if (size.y <= 0) size.y = available.y;
    ngLabelOver(text, size);
    ngDummy(size);
}

void ngSpacer(ImVec2 size) {
    ImGui::SameLine(0, 0);
    ngDummy(size);
}

void ngLabelOver(compat::RetainedStringSlice text, ImVec2 size, bool entireWindow) {
    auto* window = ImGui::GetCurrentWindow();
    auto* draw = ImGui::GetWindowDrawList();
    auto& style = ImGui::GetStyle();
    auto origin = ImGui::GetCursorScreenPos();
    const auto measured = ngMeasureString(text);
    const float xPadding = style.FramePadding.x;
    const float yPadding = style.FramePadding.y;
    if (entireWindow) {
        origin = compat::ngRequireObject(window)->OuterRectClipped.Max;
        if (size.x <= 0) size.x = window->OuterRectClipped.Min.x - window->OuterRectClipped.Max.x;
        if (size.y <= 0) size.y = window->OuterRectClipped.Min.y - window->OuterRectClipped.Max.y;
        ImGui::PushClipRect(window->OuterRectClipped.Max, window->OuterRectClipped.Min, false);
    } else {
        if (size.x <= 0) size.x = compat::ngRequireObject(window)->WorkRect.Min.x - origin.x;
        if (size.y <= 0) size.y = compat::ngRequireObject(window)->WorkRect.Min.y - origin.y;
    }
    const auto backgroundColor = ImGui::GetColorU32(ImVec4(0, 0, 0, .20f));
    const auto rounding = static_cast<float>(style.WindowRounding * .5);
    compat::ngRequireObject(draw)->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        backgroundColor, rounding);
    const ImVec2 topLeft(
        static_cast<float>(origin.x + ((size.x - (xPadding + measured.x)) * .5)),
        static_cast<float>(origin.y + ((size.y - (yPadding + measured.y)) * .5)));
    const ImVec2 bottomRight(
        static_cast<float>(origin.x + ((size.x + (xPadding + measured.x)) * .5)),
        static_cast<float>(origin.y + ((size.y + (yPadding + measured.y)) * .5)));
    draw->AddRectFilled(ImVec2(topLeft.x - 1, topLeft.y - 1), ImVec2(bottomRight.x + 1, bottomRight.y + 1),
        ImGui::GetColorU32(ImGuiCol_Text, .15f), 4);
    draw->AddRectFilled(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_WindowBg), 4);
    const auto* begin = text.data();
    const auto* end = begin ? begin + text.size() : begin;
    draw->AddText(ImVec2(static_cast<float>(origin.x + ((size.x - measured.x) * .5)),
                        static_cast<float>(origin.y + ((size.y - measured.y) * .5))),
        ImGui::GetColorU32(ImGuiCol_Text), begin, end);
    if (entireWindow) ImGui::PopClipRect();
}

void ngDummyLabel(std::string_view text, ImVec2 area) {
    ngDummyLabel(retainDummyText(text), area);
}
void ngLabelOver(std::string_view text, ImVec2 size, bool entireWindow) {
    ngLabelOver(retainDummyText(text), size, entireWindow);
}

} // namespace nijigenerate::widgets
