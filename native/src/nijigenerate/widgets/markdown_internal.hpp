#pragma once
#include <imgui.h>
#include <string>

namespace nijigenerate::widgets::markdown::detail {
// Module-private helper; this is not a public widgets export.
void underline(ImVec4 color);
// D RangeError belongs to Error, outside the ordinary Exception family.
struct FormatRangeError { std::string message; };
}
