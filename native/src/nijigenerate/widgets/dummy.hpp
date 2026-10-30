#pragma once
// Copyright 2020-2023 Inochi2D Project; 2024 nijigenerate Project.
// Author: Luna Nielsen. BSD-2-Clause; see LICENSE.
#include <imgui.h>
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::widgets {
void ngDummy(ImVec2 size = ImVec2(0, 0));
void ngDummyLabel(compat::RetainedStringSlice text, ImVec2 area = ImVec2(0, 0));
void ngLabelOver(compat::RetainedStringSlice text, ImVec2 size = ImVec2(0, 0), bool entireWindow = false);
ImVec2 ngMeasureString(compat::RetainedStringSlice text);
// Legacy byte views are copied into the same retained primary at the boundary.
void ngDummyLabel(std::string_view text, ImVec2 area = ImVec2(0, 0));
void ngLabelOver(std::string_view text, ImVec2 size = ImVec2(0, 0), bool entireWindow = false);
void ngSpacer(ImVec2 size);
ImVec2 ngAvailableSpace();
ImVec2 ngMeasureString(std::string_view text);
namespace dummy {
using ::nijigenerate::widgets::ngDummy;
using ::nijigenerate::widgets::ngDummyLabel;
using ::nijigenerate::widgets::ngLabelOver;
using ::nijigenerate::widgets::ngSpacer;
using ::nijigenerate::widgets::ngAvailableSpace;
using ::nijigenerate::widgets::ngMeasureString;
}
} // namespace nijigenerate::widgets
