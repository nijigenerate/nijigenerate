#pragma once

#include <math/mat4.hpp>
#include <array>

namespace nijigenerate::core::math {

// Match inmath's Matrix * Vector fold, including the fourth output lane and
// the initial zero. This is not the affine shortcut in Mat4::transformPoint.
inline nicxlive::core::math::Vec3 ngTransformHomogeneousPoint(
    const nicxlive::core::math::Mat4& matrix, nicxlive::core::math::Vec3 point) {
    const std::array<float, 4> input{point.x, point.y, point.z, 1.0f};
    std::array<float, 4> result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            const float product = nicxlive::core::math::detail::inmathProduct(matrix[row][column], input[column]);
            result[row] = nicxlive::core::math::detail::inmathAccumulate(result[row], product);
        }
    }
    return {result[0], result[1], result[2]};
}

} // namespace nijigenerate::core::math
