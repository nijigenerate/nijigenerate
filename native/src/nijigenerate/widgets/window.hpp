#pragma once
#include <imgui.h>

namespace nijigenerate::widgets {
bool ngBegin(const char* name, bool* open, ImGuiWindowFlags flags);
void ngEnd();
}
