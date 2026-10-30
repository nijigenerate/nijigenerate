#pragma once

#include <imgui.h>
#include <limits>

namespace nijigenerate::widgets::detail {
struct ViewportToolAreaData {
    ImVec2 contentSize{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
};
} // namespace nijigenerate::widgets::detail
