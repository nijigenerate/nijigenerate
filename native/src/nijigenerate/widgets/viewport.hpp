#pragma once

#include <imgui.h>
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::widgets {

void ngBeginViewportToolArea(compat::RetainedStringSlice id, ImVec2 pos, bool pad = true);
void ngBeginViewportToolArea(compat::RetainedStringSlice id, ImGuiDir horizontal,
    ImGuiDir vertical = ImGuiDir_Up, bool pad = true, ImVec2* pos = nullptr);
// Native counted bytes are owned before entering the actual Source body.
void ngBeginViewportToolArea(std::string_view id, ImVec2 pos, bool pad = true);
void ngBeginViewportToolArea(std::string_view id, ImGuiDir horizontal,
    ImGuiDir vertical = ImGuiDir_Up, bool pad = true, ImVec2* pos = nullptr);
void ngEndViewportToolArea();

namespace viewport {
using ::nijigenerate::widgets::ngBeginViewportToolArea;
using ::nijigenerate::widgets::ngEndViewportToolArea;
}

} // namespace nijigenerate::widgets
