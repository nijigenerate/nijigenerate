#include "detail/geometry_scalar.hpp"
#include "geometry.hpp"
#include "detail/geometry_axis.hpp"
#include "detail/geometry_kernel.hpp"
#include "detail/geometry_guide_sort.hpp"
#include "linear.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <string_view>

namespace nijigenerate::autorig::deterministic::geometry::detail {
double cross(Point2 a, Point2 b) { return a[0] * b[1] - a[1] * b[0]; }
Point2 sub(Point2 a, Point2 b) { return {a[0] - b[0], a[1] - b[1]}; }
double squared(Point2 a) { return a[0] * a[0] + a[1] * a[1]; }
Point2 guideBase(Point2 uv, const compat::FixedArray<double, 4>& bounds) {
    return {bounds[0] + uv[0] * (bounds[2] - bounds[0]),
        bounds[1] + uv[1] * (bounds[3] - bounds[1])};
}
}

namespace nijigenerate::autorig::deterministic::geometry {
compat::Slice<contracts::Point3> ngRigRotate(compat::Slice<contracts::Point3> points,
    contracts::Point3 pivot, contracts::Point3 angles) {
    for (double angle : angles)
        if (!std::isfinite(angle))
            throw compat::ThrowableException{contracts::detail::copyContractMessage("Nonfinite rig angle"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 253};
    constexpr long double pi = 0x1.921fb54442d18469898cc51701b84p+1L;
    const double yaw = static_cast<double>(static_cast<long double>(angles[0]) * pi / 180.L);
    const double pitch = static_cast<double>(static_cast<long double>(angles[1]) * pi / 180.L);
    const double roll = static_cast<double>(static_cast<long double>(angles[2]) * pi / 180.L);
    compat::Slice<contracts::Point3> result;
    for (auto point : points) {
        if (angles[0] == 0. && angles[1] == 0. && angles[2] == 0.) {
            result.append(point);
            continue;
        }
        contracts::Point3 p;
        for (std::size_t i = 0; i < 3; ++i) p[i] = point[i] - pivot[i];
        const double y = std::cos(pitch) * p[1] - std::sin(pitch) * p[2];
        double z = std::sin(pitch) * p[1] + std::cos(pitch) * p[2];
        const double x = std::cos(yaw) * p[0] + std::sin(yaw) * z;
        z = -std::sin(yaw) * p[0] + std::cos(yaw) * z;
        result.append(contracts::Point3{std::cos(roll) * x - std::sin(roll) * y + pivot[0],
            std::sin(roll) * x + std::cos(roll) * y + pivot[1], z + pivot[2]});
    }
    return result;
}
compat::Slice<contracts::Point2> ngRigFitGuide(compat::Slice<contracts::Point2> query,
    compat::FixedArray<double, 4> bounds, compat::Slice<contracts::Point2> source,
    compat::Slice<contracts::Point2> target) {
    using namespace detail;
    using contracts::Point2;
    using namespace linear;
    if (!(bounds[2] > bounds[0] && bounds[3] > bounds[1] && source.size() == target.size()))
        throw compat::ThrowableException{
            contracts::detail::copyContractMessage("Invalid guide bounds or landmarks"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 193};
    const auto sourceCount = source.size();
    compat::Slice<std::size_t> order;
    for (std::size_t i = 0; i < sourceCount; ++i) order.append(i);
    guideSortOrder({order.data(), order.size()}, [&](std::size_t a, std::size_t b) {
        return source[a][0] < source[b][0]
            || (source[a][0] == source[b][0] && source[a][1] < source[b][1]);
    });
    compat::Slice<Point2> controls, delta;
    const auto equal = [](Point2 a, Point2 b) { return a[0] == b[0] && a[1] == b[1]; };
    for (const auto i : order) {
        const auto p = source[i];
        if (!(p[0] >= 0 && p[0] <= 1 && p[1] >= 0 && p[1] <= 1))
            throw compat::ThrowableException{contracts::detail::copyContractMessage("Landmark outside material UV"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 204};
        const auto d = sub(target[i], guideBase(p, bounds));
        if (controls.size() && equal(controls[controls.size() - 1], p)) {
            if (!equal(delta[delta.size() - 1], d))
                throw compat::ThrowableException{
                    contracts::detail::copyContractMessage("Conflicting landmark targets"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 207};
            continue;
        }
        controls.append(p); delta.append(d);
    }
    bool affineRank = false;
    if (controls.size() >= 3) for (std::size_t i = 2; i < controls.size(); ++i)
        if (std::abs(cross(sub(controls[1], controls[0]), sub(controls[i], controls[0]))) > 1e-12)
            affineRank = true;
    RigMatrix coefficients;
    const auto n = controls.size();
    if (affineRank) {
        auto matrix = ngRigZeroMatrix(n + 3, n + 3), right = ngRigZeroMatrix(n + 3, 2);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j)
                matrix[i][j] = kernel(squared(sub(controls[i], controls[j])));
            const std::array<double, 3> polynomial{1.0, controls[i][0], controls[i][1]};
            for (std::size_t j = 0; j < 3; ++j) matrix[i][n + j] = matrix[n + j][i] = polynomial[j];
            for (std::size_t axis = 0; axis < 2; ++axis) right[i][axis] = delta[i][axis];
        }
        coefficients = ngRigSolve(matrix, right);
    }
    compat::Slice<Point2> result;
    for (const auto p : query) {
        if (!(p[0] >= 0 && p[0] <= 1 && p[1] >= 0 && p[1] <= 1))
            throw compat::ThrowableException{contracts::detail::copyContractMessage("Query outside material UV"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 228};
        auto value = guideBase(p, bounds);
        if (affineRank) for (std::size_t axis = 0; axis < 2; ++axis) {
            value[axis] += coefficients[n][axis] + p[0] * coefficients[n + 1][axis]
                + p[1] * coefficients[n + 2][axis];
            for (std::size_t i = 0; i < n; ++i)
                value[axis] += kernel(squared(sub(p, controls[i]))) * coefficients[i][axis];
        } else if (n) {
            double closest = std::numeric_limits<double>::infinity();
            for (const auto q : controls) {
                const auto distance = squared(sub(p, q));
                closest = distance < closest ? distance : closest;
            }
            double total = 0;
            Point2 weighted{0.0, 0.0};
            for (std::size_t i = 0; i < controls.size(); ++i) {
                const double distance = squared(sub(p, controls[i]));
                if (distance == 0) { weighted = delta[i]; total = 1; break; }
                const double weight = closest / distance;
                total += weight;
                for (std::size_t axis = 0; axis < 2; ++axis) weighted[axis] += weight * delta[i][axis];
            }
            for (std::size_t axis = 0; axis < 2; ++axis) value[axis] += weighted[axis] / total;
        }
        for (const auto coordinate : value)
            if (!std::isfinite(coordinate))
                throw compat::ThrowableException{contracts::detail::copyContractMessage("Nonfinite fitted guide"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 245};
        result.append(value);
    }
    return result;
}

compat::Slice<contracts::Point2> ngRigUVGrid(compat::Slice<double> xs, compat::Slice<double> ys) {
    detail::lines(xs); detail::lines(ys);
    compat::Slice<contracts::Point2> result;
    for (const auto y : ys) for (const auto x : xs) result.append(contracts::Point2{x, y});
    return result;
}

compat::Slice<contracts::Point2> ngRigSampleGrid(compat::Slice<contracts::Point2> grid,
    compat::Slice<double> xs, compat::Slice<double> ys, compat::Slice<contracts::Point2> query) {
    detail::lines(xs); detail::lines(ys);
    if (grid.size() != xs.size() * ys.size())
        throw compat::ThrowableException{contracts::detail::copyContractMessage("Grid dimensions differ from axes"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 174};
    compat::Slice<contracts::Point2> result;
    for (const auto p : query) {
        if (!(p[0] >= 0 && p[0] <= 1 && p[1] >= 0 && p[1] <= 1))
            throw compat::ThrowableException{contracts::detail::copyContractMessage("Query is outside material UV"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 177};
        std::size_t x = 0, y = 0;
        while (x + 2 < xs.size() && p[0] > xs[x + 1]) ++x;
        while (y + 2 < ys.size() && p[1] > ys[y + 1]) ++y;
        const double u = (p[0] - xs[x]) / (xs[x + 1] - xs[x]);
        const double v = (p[1] - ys[y]) / (ys[y + 1] - ys[y]);
        contracts::Point2 sample{0.0, 0.0};
        for (std::size_t axis = 0; axis < 2; ++axis) sample[axis] =
            (1 - v) * ((1 - u) * grid[y * xs.size() + x][axis] + u * grid[y * xs.size() + x + 1][axis])
            + v * ((1 - u) * grid[(y + 1) * xs.size() + x][axis]
                + u * grid[(y + 1) * xs.size() + x + 1][axis]);
        result.append(sample);
    }
    return result;
}

contracts::Point3 ngRigClosestTriangleWeights(contracts::Point2 p, contracts::Point2 a,
    contracts::Point2 b, contracts::Point2 c) {
    using namespace detail;
    const auto ab = sub(b, a), ac = sub(c, a), ap = sub(p, a);
    const double determinant = cross(ab, ac);
    if (determinant != 0) {
        const double v = cross(ap, ac) / determinant, w = cross(ab, ap) / determinant;
        if (v >= 0 && w >= 0 && v + w <= 1) return {1 - v - w, v, w};
    }
    const std::array<contracts::Point2, 3> vertices{a, b, c};
    contracts::Point3 best{1.0, 0.0, 0.0};
    double closest = squared(sub(p, a));
    for (std::size_t i = 0; i < 3; ++i) {
        const std::size_t j = (i + 1) % 3;
        const auto edge = sub(vertices[j], vertices[i]);
        const double length = squared(edge);
        double t = 0;
        if (length != 0) {
            const double value = (sub(p, vertices[i])[0] * edge[0]
                + sub(p, vertices[i])[1] * edge[1]) / length;
            // Phobos double clamp returns an unordered input unchanged.
            t = value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value;
        }
        const contracts::Point2 q{vertices[i][0] + t * edge[0], vertices[i][1] + t * edge[1]};
        const double distance = squared(sub(p, q));
        if (distance < closest) {
            closest = distance;
            for (auto& value : best) value = 0;
            best[i] = 1 - t;
            best[j] = t;
        }
    }
    return best;
}
}
