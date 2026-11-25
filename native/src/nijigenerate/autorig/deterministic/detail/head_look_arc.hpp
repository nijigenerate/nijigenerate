#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace nijigenerate::autorig::deterministic::head_look {

// Both angular limits must be finite and strictly between zero and pi / 2.
// The caller supplies the effective limits for the current input quadrant.
class Arc {
public:
    Arc(double yawRadians, double pitchRadians)
        : yawRadians_(yawRadians), pitchRadians_(pitchRadians),
          horizontal_(std::sin(yawRadians)), vertical_(std::sin(pitchRadians)) {
        auto previous = rimDirection(0.);
        for (std::size_t i = 1; i <= arcSteps; ++i) {
            const auto direction = rimDirection(static_cast<double>(i) * quarter / arcSteps);
            const double crossX = previous[1] * direction[2] - previous[2] * direction[1];
            const double crossY = previous[2] * direction[0] - previous[0] * direction[2];
            const double crossZ = previous[0] * direction[1] - previous[1] * direction[0];
            const double product = previous[0] * direction[0] + previous[1] * direction[1]
                + previous[2] * direction[2];
            arcLengths_[i] = arcLengths_[i - 1]
                + std::atan2(std::hypot(crossX, crossY, crossZ), product);
            previous = direction;
        }
    }

    // Return editor angles {pitch, yaw, roll}, in radians. The depth runtime
    // interprets them as Rz(roll) * Ry(-yaw) * Rx(-pitch) in the bone's rest frame.
    std::array<double, 3> euler(double u, double v) const {
        u = std::clamp(u, -1., 1.);
        v = std::clamp(v, -1., 1.);
        // Preserve every cardinal input exactly, including the neutral pose.
        if (u == 0.) return {v * pitchRadians_, 0., 0.};
        if (v == 0.) return {0., u * yawRadians_, 0.};
        const double rho = std::max(std::abs(u), std::abs(v));
        if (rho < 1e-12) return {0., 0., 0.};

        const double phi = uniformBearing(std::atan2(std::abs(v), std::abs(u)));
        const auto rim = rimDirection(phi);
        const double theta = rho * std::atan2(std::hypot(rim[0], rim[1]), rim[2]);
        const double sine = std::sin(theta);
        // Reflect the preview's X/Y signs into the existing bone-local frame.
        const double directionX = -std::copysign(sine * std::cos(phi), u);
        const double directionY = std::copysign(sine * std::sin(phi), v);
        const double directionZ = std::cos(theta);

        // The shortest swing from +Z has quaternion (1 + z, -y, x, 0).
        double w = 1. + directionZ;
        double x = -directionY;
        double y = directionX;
        const double length = std::hypot(w, x, y);
        w /= length;
        x /= length;
        y /= length;

        // Decompose that swing using the depth runtime's signed Euler order.
        const double m00 = 1. - 2. * y * y;
        const double m10 = 2. * x * y;
        const double m20 = -2. * w * y;
        const double m21 = 2. * w * x;
        const double m22 = 1. - 2. * (x * x + y * y);
        return {-std::atan2(m21, m22), std::asin(std::clamp(m20, -1., 1.)),
            std::atan2(m10, m00)};
    }

private:
    static constexpr std::size_t arcSteps = 2048;
    static constexpr double quarter = std::numbers::pi / 2.;

    std::array<double, 3> rimDirection(double phi) const {
        const double cosine = std::cos(phi);
        const double sine = std::sin(phi);
        const double radius = 1. / std::hypot(cosine / horizontal_, sine / vertical_);
        return {radius * cosine, -radius * sine,
            std::sqrt(std::max(0., 1. - radius * radius))};
    }

    double uniformBearing(double phi) const {
        const double target = std::clamp(phi / quarter, 0., 1.) * arcLengths_[arcSteps];
        if (target <= 0.) return 0.;
        if (target >= arcLengths_[arcSteps]) return quarter;
        std::size_t lo = 0;
        std::size_t hi = arcSteps;
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            if (arcLengths_[mid] < target) lo = mid;
            else hi = mid;
        }
        const double span = arcLengths_[hi] - arcLengths_[lo];
        const double fraction = span > 0. ? (target - arcLengths_[lo]) / span : 0.;
        return (static_cast<double>(lo) + fraction) * quarter / arcSteps;
    }

    double yawRadians_;
    double pitchRadians_;
    double horizontal_;
    double vertical_;
    std::array<double, arcSteps + 1> arcLengths_{};
};

} // namespace nijigenerate::autorig::deterministic::head_look
