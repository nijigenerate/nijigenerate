#pragma once
#include <imgui.h>
struct ImRect;
namespace nijigenerate::widgets {
ImDrawList* ngCreateWindowDrawList();
void ngDestroyWindowDrawList(ImDrawList* drawList);
// Call at window level, before table/columns or externally managed channels.
// The scratch list must be distinct and use this ImGui context's shared data.
// Original rectangles/defaults are retained; native command/index bookkeeping
// follows the explicitly approved rendering-consistent M311 amendment.
void ngRenderWindowShadow(ImDrawList* drawList, ImRect area, float falloff=16, float startShade=.25f);
}
