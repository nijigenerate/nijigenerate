#include "controls_skin.hpp"
#include "controls_numeric.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <limits>
#include <span>
#include <algorithm>
#include <array>
#include <cmath>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::autorig::deterministic::controls_detail {
namespace {

double clampCoordinate(double value, double lower, double upper) {
    // Source Phobos checks lower > upper, allowing unordered NaN bounds.
    // Keep source release comparisons without std::clamp's ordered-bound precondition.
#ifndef NDEBUG
    if (lower > upper)
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Lower can't be greater than upper."),
            compat::RetainedString::fromStaticLiteral("std/algorithm/comparison.d"), 588);
#endif
    if (value < lower) return lower;
    if (value > upper) return upper;
    return value;
}

} // namespace

double SkinProfile::midline(double level) {
    const auto last = [](compat::Slice<double> values, std::size_t line) {
        const auto index = values.size() - 1;
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
        return values[index];
    };
    compat::Slice<double> axis;
    axis.resize(3);
    axis[0] = eye;
    axis[1] = mouth;
    axis[2] = last(y, 573);
    compat::Slice<double> centers;
    centers.resize(3);
    centers[0] = (eyeSpan[0] + eyeSpan[1]) / 2;
    centers[1] = (mouthSpan[0] + mouthSpan[1]) / 2;
    const double leftLast = last(left, 574);
    const double rightLast = last(right, 574);
    centers[2] = (leftLast + rightLast) / 2;
    return curveSample(std::span<const double>(axis.data(), axis.size()),
        std::span<const double>(centers.data(), centers.size()), level);
}

contracts::Point3 barycentric(contracts::Point2 point, contracts::Point2 first,
    contracts::Point2 second, contracts::Point2 third) {
    const double determinant = (second[0] - first[0]) * (third[1] - first[1]) -
        (second[1] - first[1]) * (third[0] - first[0]);
    if (determinant == 0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return {nan, nan, nan};
    }
    const double u = ((point[0] - first[0]) * (third[1] - first[1]) -
        (point[1] - first[1]) * (third[0] - first[0])) / determinant;
    const double v = ((second[0] - first[0]) * (point[1] - first[1]) -
        (second[1] - first[1]) * (point[0] - first[0])) / determinant;
    return {1 - u - v, u, v};
}

compat::Slice<bool> cheekSupport(compat::Slice<contracts::Point2> world,
    core::settings::SettingsJsonValue triangles, SkinProfile profile, int side) {
    compat::Slice<contracts::Point2> points;
    compat::Slice<bool> allowed;
    for (const auto p : world) {
        const auto q = profile.frame.local(p);
        points.append(q);
        allowed.append(side * (q[0] - profile.midline(q[1])) > 0 &&
            q[1] >= profile.temple && q[1] <= profile.y[profile.y.size() - 1]);
    }
    const auto near = allowed.dup();
    for (const auto triangle : triangles.as_array()) {
        auto indices = contracts::ngRigNumbers(triangle);
        bool outside = false;
        for (const double index : indices) if (!near[sourceControlIndex(index)]) outside = true;
        const double levels[3]{profile.eye, profile.mouth, profile.y[profile.y.size() - 1]};
        const std::array<std::size_t, 2> edges[3]{{0, 1}, {1, 2}, {2, 0}};
        for (const double level : levels) for (const auto edge : edges) {
            const auto p = points[sourceControlIndex(indices[edge[0]])];
            const auto q = points[sourceControlIndex(indices[edge[1]])];
            if (level < std::min(p[1], q[1]) || level > std::max(p[1], q[1])) continue;
            const double t = std::abs(q[1] - p[1]) > 1e-12 ? (level - p[1]) / (q[1] - p[1]) : 0;
            if (side * (p[0] + t * (q[0] - p[0]) - profile.midline(level)) <= 0) outside = true;
        }
        if (outside) for (const double index : indices) allowed[sourceControlIndex(index)] = false;
    }
    return allowed;
}

contracts::Point2 gridSample(compat::Slice<contracts::Point2> values,
    compat::Slice<double> xs, compat::Slice<double> ys, contracts::Point2 point) {
    if (values.size() != xs.size() * ys.size())
        throw compat::ThrowableException(
            contracts::detail::copyContractMessage("Invalid native face grid dimensions"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), 608);
    point = contracts::Point2{clampCoordinate(point[0], xs[0], xs[xs.size() - 1]),
        clampCoordinate(point[1], ys[0], ys[ys.size() - 1])};
    std::size_t x = 0, y = 0;
    while (x + 2 < xs.size() && point[0] > xs[x + 1]) ++x;
    while (y + 2 < ys.size() && point[1] > ys[y + 1]) ++y;
    const double u = (point[0] - xs[x]) / (xs[x + 1] - xs[x]);
    const double v = (point[1] - ys[y]) / (ys[y + 1] - ys[y]);
    contracts::Point2 result;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        result[axis] = (1 - v) * ((1 - u) * values[y * xs.size() + x][axis] +
            u * values[y * xs.size() + x + 1][axis]) +
            v * ((1 - u) * values[(y + 1) * xs.size() + x][axis] +
                u * values[(y + 1) * xs.size() + x + 1][axis]);
    }
    return result;
}

} // namespace nijigenerate::autorig::deterministic::controls_detail
