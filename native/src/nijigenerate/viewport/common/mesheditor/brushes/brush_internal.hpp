#pragma once
#include "base.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace nijigenerate::viewport::common::mesheditor::brushes::detail {
using namespace nicxlive::core::math;
inline double distance(Vec2 a, Vec2 b) {
    // inmath lengthSquared accumulates float squares into real.
    const float x = a.x - b.x, y = a.y - b.y;
    const float xx = x * x, yy = y * y;
    // Keep the real-valued result through the caller's comparison/arithmetic.
    // D real is binary64 on the selected LDC Windows MSVC target.
    return std::sqrt(static_cast<double>(xx) + static_cast<double>(yy));
}
inline compat::Slice<float> weights(base::Brush& brush, Vec2 center, Vec2Array positions) {
    compat::Slice<float> result;
    for (std::size_t i = 0; i < positions.size(); ++i)
        result.append(brush.weightAt(center, positions[i].toVector()));
    return result;
}
inline void cross(Vec3Array& points, Vec2 center, float radius) {
    points.append(Vec3{center.x - radius, center.y, 0});
    points.append(Vec3{center.x + radius, center.y, 0});
    points.append(Vec3{center.x, center.y - radius, 0});
    points.append(Vec3{center.x, center.y + radius, 0});
}
inline void draw(const Vec3Array& points, Mat4 transform) {
    core::dbg::inDbgSetBuffer(points);
    core::dbg::inDbgPointsSize(8);
    core::dbg::inDbgDrawLines(Vec4{0, 0, 0, 1}, transform);
    core::dbg::inDbgPointsSize(4);
    core::dbg::inDbgDrawLines(Vec4{1, 0, 0, 1}, transform);
}
inline void control(const char* id, const char* label, float& value, float low, float high) {
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(64);
    widgets::ngDragFloat(label, &value, 1, low, high, "%.2f", ImGuiSliderFlags_NoRoundToFormat);
    ImGui::PopID();
}
}
