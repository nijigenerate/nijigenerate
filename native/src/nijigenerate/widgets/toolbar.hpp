#pragma once
// Copyright 2020-2023 Inochi2D Project; 2024 nijigenerate Project.
// Author: Luna Nielsen. BSD-2-Clause; see LICENSE.
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::widgets {
bool ngBeginInnerToolbar(float height, bool matchTitlebar = false, bool offset = true);
void ngEndInnerToolbar();
bool ngToolbarButton(const char* text, float width = 0);
void ngToolbarSpacer(float space);
void ngToolbarSeparator();
void ngToolbarText(compat::RetainedStringSlice text);
// Legacy counted bytes enter the same retained primary at the boundary.
void ngToolbarText(std::string_view text);
} // namespace nijigenerate::widgets
namespace nijigenerate::widgets::toolbar {
using ::nijigenerate::widgets::ngBeginInnerToolbar;
using ::nijigenerate::widgets::ngEndInnerToolbar;
using ::nijigenerate::widgets::ngToolbarButton;
using ::nijigenerate::widgets::ngToolbarSpacer;
using ::nijigenerate::widgets::ngToolbarSeparator;
using ::nijigenerate::widgets::ngToolbarText;
}
