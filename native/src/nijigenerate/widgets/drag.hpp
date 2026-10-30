#pragma once
#include <imgui.h>
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>
namespace nijigenerate::widgets {
bool ngDragFloat(compat::RetainedStringSlice id, float* value, float adjustSpeed, float minValue, float maxValue,
    compat::RetainedStringSlice format, ImGuiSliderFlags flags=ImGuiSliderFlags_None);
bool ngGetHasDragState(compat::RetainedStringSlice id);
float ngGetDragFloatInitialValue(compat::RetainedStringSlice id);
// Native compatibility views borrow storage, including the format's C-string suffix.
bool ngDragFloat(std::string_view id, float* value, float adjustSpeed, float minValue, float maxValue,
    std::string_view format, ImGuiSliderFlags flags=ImGuiSliderFlags_None);
bool ngGetHasDragState(std::string_view id);
float ngGetDragFloatInitialValue(std::string_view id);
namespace drag {
using ::nijigenerate::widgets::ngDragFloat;
using ::nijigenerate::widgets::ngGetHasDragState;
using ::nijigenerate::widgets::ngGetDragFloatInitialValue;
}
}
