#pragma once
#include <imgui.h>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <optional>
#include <string>
#include <string_view>
namespace nijigenerate::widgets {
using InputTextString=std::optional<std::string>;
// Host compatibility entries borrow ID/label storage for the complete operation.
// Source callers use the retained descriptor entries below.
bool ngInputText(std::string_view id, InputTextString& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, float width, InputTextString& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, std::string_view label, InputTextString& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, std::string_view label, float width, InputTextString& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputTextMultiline(std::string_view id, InputTextString& buffer, ImVec2 size,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
// Actual retained draft slots share the same widget/callback/store ordering.
bool ngInputText(std::string_view id, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, float width, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, std::string_view label, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, std::string_view label, float width,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputTextMultiline(std::string_view id, compat::RetainedStringSlice& buffer, ImVec2 size,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, compat::RetainedStringSlice label,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(std::string_view id, compat::RetainedStringSlice label, float width,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label, float width,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, float width, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputTextMultiline(compat::RetainedStringSlice id, compat::RetainedStringSlice& buffer, ImVec2 size,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedString& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, float width, compat::RetainedString& buffer,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label,
    compat::RetainedString& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label, float width,
    compat::RetainedString& buffer, ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
bool ngInputTextMultiline(compat::RetainedStringSlice id, compat::RetainedString& buffer, ImVec2 size,
    ImGuiInputTextFlags flags=ImGuiInputTextFlags_None);
namespace inputtext {
using ::nijigenerate::widgets::ngInputText;
using ::nijigenerate::widgets::ngInputTextMultiline;
}
}
