// Copyright 2020-2023 Inochi2D Project; 2024 nijigenerate Project.
// Author: Luna Nielsen. BSD-2-Clause; see LICENSE.
#include <nijigenerate/widgets/lock.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui.h>
namespace nijigenerate::widgets {
namespace {
bool lockButton(bool* value, const char* origin) {
    ImGui::SameLine(0, 0);
    ImGui::PushID(origin);
    ImGui::PushItemWidth(16);
    ngText(*compat::ngRequireObject(value) ? "\xEE\xA2\x99" : "\xEE\xA2\x98");
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (clicked) *value = !*value;
    ImGui::PopItemWidth();
    ImGui::PopID();
    return clicked;
}
}
bool ngLockButton(bool* value, compat::RetainedStringSlice origin) {
    return lockButton(value, origin.data());
}
bool ngLockButton(bool* value, std::string_view origin) {
    return lockButton(value, origin.data());
}
}
