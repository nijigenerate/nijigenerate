#pragma once
#include <imgui.h>
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::widgets {
bool ngToggleSwitch(compat::RetainedStringSlice id, bool& value, ImVec2 size = ImVec2(36, 20));
// Native compatibility views borrow caller bytes for the whole operation.
bool ngToggleSwitch(std::string_view id, bool& value, ImVec2 size = ImVec2(36, 20));
}
namespace nijigenerate::widgets::toggle {
using ::nijigenerate::widgets::ngToggleSwitch;
}
