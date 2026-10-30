#include "physics.hpp"
#include "physics_math_detail.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>

namespace nijigenerate::autorig::deterministic::physics {
namespace {

using detail::sub;
using detail::norm;

} // namespace

RigPhysicsFrame ngRigPhysicsFrame(Point2 fixed, Point2 free) {
    auto direction = sub(free, fixed);
    const auto length = norm(direction);
    if (!std::isfinite(length) || !(length > 1e-6))
        detail::failEnforcement("Degenerate physics support-to-free frame", 73);
    for (auto& component : direction) component /= length;
    return RigPhysicsFrame{fixed, Point2{direction[1], -direction[0]}, direction, length};
}

bool ngRigPhysicsNoInversion(compat::Slice<Point2> vertices, compat::Slice<std::uint32_t> indices,
                             compat::Slice<Point2> values, double scale) {
    if (vertices.size() != values.size() || indices.size() % 3 != 0)
        detail::failEnforcement("Invalid physics topology arrays", 393);
    const auto area = [](Point2 a, Point2 b, Point2 c) {
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    };
    for (std::size_t index = 0; index < indices.size(); index += 3) {
        std::array<Point2, 3> base, posed;
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const auto id = indices[index + corner];
            if (id >= vertices.size()) detail::failEnforcement("Physics triangle outside mesh", 398);
            base[corner] = vertices[id];
            posed[corner] = Point2{base[corner][0] + values[id][0] * scale,
                                  base[corner][1] + values[id][1] * scale};
        }
        const auto before = area(base[0], base[1], base[2]);
        if (std::abs(before) > 1e-8 && !(before * area(posed[0], posed[1], posed[2]) > 0.0)) return false;
    }
    return true;
}

Point2 ngRigPhysicsTransform(PhysicsAffineTransform matrix, Point2 point, bool direction) {
    return Point2{
        matrix[0] * point[0] + matrix[1] * point[1] + (direction ? 0.0 : matrix[2]),
        matrix[3] * point[0] + matrix[4] * point[1] + (direction ? 0.0 : matrix[5])
    };
}

PhysicsAffineTransform ngRigPhysicsInverse(PhysicsAffineTransform matrix) {
    const auto determinant = matrix[0] * matrix[4] - matrix[1] * matrix[3];
    if (!(std::abs(determinant) > 1e-20)) detail::failEnforcement("Singular physics frame", 413);
    PhysicsAffineTransform result{matrix[4] / determinant, -matrix[1] / determinant, 0.0,
                                 -matrix[3] / determinant, matrix[0] / determinant, 0.0};
    const auto shift = ngRigPhysicsTransform(result, Point2{matrix[2], matrix[5]}, true);
    result[2] = -shift[0];
    result[5] = -shift[1];
    return result;
}

} // namespace nijigenerate::autorig::deterministic::physics
