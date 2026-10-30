#pragma once

#include <imgui.h>
#include <nijigenerate/compat/retained_string.hpp>
#include <limits>
#include <string_view>

namespace nijigenerate::widgets {

extern const ImVec4 colorUndefined;
ImVec4 ngButtonTextColor();
void ngButtonTextColor(ImVec4 color);
bool ngDropdownButtonIconRetained(compat::RetainedString id, compat::RetainedString icon,
                          ImVec2 size = ImVec2(-1, -1), bool open = false);
bool ngDropdownButtonRetained(compat::RetainedString id, ImVec2 size = ImVec2(-1, -1), bool open = false);
bool ngBeginDropdownMenuRetained(compat::RetainedString id,
                         compat::RetainedString icon = compat::RetainedString::emptyLiteral(),
                         ImVec2 minimum = ImVec2(192, 0),
                         ImVec2 maximum = ImVec2(192, std::numeric_limits<float>::max()));
bool ngButtonColored(const char* text, ImVec2 size = ImVec2(0, 0), ImVec4 textColor = colorUndefined);
bool ngDropdownButtonIcon(std::string_view id, std::string_view icon,
                          ImVec2 size = ImVec2(-1, -1), bool open = false);
bool ngDropdownButton(std::string_view id, ImVec2 size = ImVec2(-1, -1), bool open = false);
bool ngBeginDropdownMenu(std::string_view id, std::string_view icon = "",
                         ImVec2 minimum = ImVec2(192, 0),
                         ImVec2 maximum = ImVec2(192, std::numeric_limits<float>::max()));
void ngEndDropdownMenu();
bool ngBeginTabItem(const char* text, bool* open = nullptr, ImGuiTabItemFlags flags = ImGuiTabItemFlags_None);
void ngEndTabItem();
bool ngCheckbox(const char* text, bool* value);

namespace detail { struct DropDownMenuData; }

namespace button {
using ::nijigenerate::widgets::colorUndefined;
using ::nijigenerate::widgets::ngButtonTextColor;
using ::nijigenerate::widgets::ngButtonColored;
using ::nijigenerate::widgets::ngDropdownButtonIcon;
using ::nijigenerate::widgets::ngDropdownButton;
using ::nijigenerate::widgets::ngBeginDropdownMenu;
using ::nijigenerate::widgets::ngEndDropdownMenu;
using ::nijigenerate::widgets::ngBeginTabItem;
using ::nijigenerate::widgets::ngEndTabItem;
using ::nijigenerate::widgets::ngCheckbox;
using ::nijigenerate::widgets::ngDropdownButtonIconRetained;
using ::nijigenerate::widgets::ngDropdownButtonRetained;
using ::nijigenerate::widgets::ngBeginDropdownMenuRetained;
}

} // namespace nijigenerate::widgets
