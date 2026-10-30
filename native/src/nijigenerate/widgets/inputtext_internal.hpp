#pragma once
#include <common/shared_array.hpp>
#include <imgui.h>
namespace nijigenerate::widgets::inputtext::detail {
using WorkingBuffer=::nicxlive::core::common::SharedArray<char>;
struct TextCallbackUserData { WorkingBuffer* buf=nullptr; };
extern "C" int ngInputTextResizeCallback(ImGuiInputTextCallbackData* data);
extern "C" int ngLabeledInputTextResizeCallback(ImGuiInputTextCallbackData* data);
extern "C" int ngMultilineInputTextResizeCallback(ImGuiInputTextCallbackData* data);
}
