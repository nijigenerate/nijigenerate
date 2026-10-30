#pragma once

#include <math/types.hpp>
#include <cmath>

namespace nijigenerate::commands::depth::bone {
namespace {

float segmentLength(nicxlive::core::math::Vec3 a, nicxlive::core::math::Vec3 b) {
    const nicxlive::core::math::Vec3 delta{b.x - a.x, b.y - a.y, b.z - a.z};
    return std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

nicxlive::core::math::Vec3 normalizeVec(nicxlive::core::math::Vec3 value,
    nicxlive::core::math::Vec3 fallback = {0.0f, 1.0f, 0.0f}) {
    const auto length = segmentLength({0.0f, 0.0f, 0.0f}, value);
    if (length <= 1e-8f) return fallback;
    return {value.x / length, value.y / length, value.z / length};
}

float dotVec(nicxlive::core::math::Vec3 a, nicxlive::core::math::Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace
} // namespace nijigenerate::commands::depth::bone
