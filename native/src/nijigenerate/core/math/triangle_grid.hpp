#pragma once

namespace nijigenerate::core::math::triangle::detail::grid_deformation {
inline float clampValue(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}
} // namespace nijigenerate::core::math::triangle::detail::grid_deformation
