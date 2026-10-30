#pragma once
#include <imgui.h>
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>
namespace nijigenerate::widgets {
bool ngBufferBar(compat::RetainedStringSlice label, float value, ImVec2 size, ImU32 background, ImU32 foreground);
bool ngSpinner(compat::RetainedStringSlice label, float radius, int thickness, ImU32 color);
// Native compatibility views borrow caller storage throughout the operation.
bool ngBufferBar(std::string_view label, float value, ImVec2 size, ImU32 background, ImU32 foreground);
bool ngSpinner(std::string_view label, float radius, int thickness, ImU32 color);
}
namespace nijigenerate::widgets::progress {
using ::nijigenerate::widgets::ngBufferBar;
using ::nijigenerate::widgets::ngSpinner;
}
