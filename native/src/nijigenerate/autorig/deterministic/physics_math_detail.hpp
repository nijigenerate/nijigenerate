#pragma once

#include "physics.hpp"
#include "physics_detail.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <cstddef>
#include <cmath>
#include <limits>
#include <source_location>
#include <utility>

namespace nijigenerate::autorig::deterministic::physics::detail {

template<std::size_t N> [[noreturn]] void failEnforcement(const char (&message)[N], std::size_t sourceLine,
        std::source_location source = std::source_location::current()) {
    physics_detail::failEnforcement(message, sourceLine, source);
}

template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t sourceLine,
        std::source_location source = std::source_location::current()) {
    if (!condition) failEnforcement(message, sourceLine, source);
}

inline double dmin(double a, double b) { return b < a ? b : a; }
inline double dmax(double a, double b) { return a < b ? b : a; }
inline double clamp(double value, double lower, double upper) {
    return value < lower ? lower : value > upper ? upper : value;
}

inline Point2 sub(Point2 a, Point2 b) {
    return Point2{a[0] - b[0], a[1] - b[1]};
}

inline double dot(Point2 a, Point2 b) {
    return a[0] * b[0] + a[1] * b[1];
}

inline double norm(Point2 point) {
    return std::sqrt(dot(point, point));
}

inline Point2 unit(Point2 point) {
    const auto length = norm(point);
    enforce(length > 1e-6, "Degenerate physics direction", 44);
    for (auto& component : point) component /= length;
    return point;
}

// Retain the real input descriptor while the separately owned scalar provider runs.
inline double percentile(compat::Slice<double> values, double fraction) {
    return physics_detail::percentile({values.data(), values.size()}, fraction);
}

inline Point2 mean(compat::Slice<Point2> points) {
    enforce(!points.empty(), "Empty physics section", 52);
    Point2 result{0.0, 0.0};
    for (auto point : points) {
        result[0] += point[0];
        result[1] += point[1];
    }
    const auto count = static_cast<double>(points.size());
    for (auto& component : result) component /= count;
    return result;
}

inline Point2 section(compat::Slice<Point2> points, compat::Slice<double> projection,
        double fraction, bool upper) {
    const auto threshold = percentile(projection, fraction);
    compat::Slice<Point2> selected;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto point = points[index];
        if (index >= projection.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, projection.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"), 58);
        const auto value = projection[index];
        if (upper ? value >= threshold : value <= threshold) selected.append(point);
    }
    return mean(selected);
}

inline double segmentDistance(Point2 point, Point2 a, Point2 b) {
    const auto direction = sub(b, a);
    const auto numerator = dot(sub(point, a), direction);
    const auto denominator = dmax(dot(direction, direction), 1e-12);
    const auto t = clamp(numerator / denominator, 0.0, 1.0);
    return norm(Point2{point[0] - a[0] - t * direction[0], point[1] - a[1] - t * direction[1]});
}

inline double chainDistance(Point2 point, compat::Slice<Point2> joints) {
    auto distance = std::numeric_limits<double>::infinity();
    for (std::size_t index = 1; index < joints.size(); ++index)
        distance = dmin(distance, segmentDistance(point, joints[index - 1], joints[index]));
    return distance;
}

} // namespace nijigenerate::autorig::deterministic::physics::detail
