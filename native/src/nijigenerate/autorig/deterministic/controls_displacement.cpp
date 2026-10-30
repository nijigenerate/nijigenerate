#include "controls_displacement.hpp"
#include <cmath>
#include <span>

namespace nijigenerate::autorig::deterministic::controls_detail {

contracts::Point2 localBendDisplacement(contracts::Point2 point, double x, double y,
    const Frame& localFrame) {
    const double curvature = x * .55 / localFrame.width;
    contracts::Point2 bent = point;
    if (std::abs(curvature) > 1e-12) {
        bent[0] = (1 / curvature - point[1]) * std::sin(point[0] * curvature);
        bent[1] = 1 / curvature - (1 / curvature - point[1]) * std::cos(point[0] * curvature);
    }
    return {bent[0] - point[0], bent[1] - point[1] + y * .1 * point[0]};
}

contracts::Point2 irisDisplacement(double x, double y, const Frame& eye, double height) {
    return {x * eye.width * .18, y * height * .18};
}

contracts::Point2 eyebrowDisplacement(contracts::Point2 point, double x, double y, const Frame& eye) {
    return {0., x * eye.width * .08 + y * point[0] * .15};
}

contracts::Point2 mouthDisplacement(contracts::Point2 point, double x, double expression,
    core::settings::SettingsJsonValue material, bool rigid, Frame& mouthFrame, double height) {
    double basis = point[1];
    if (rigid && x >= 0) {
        compat::Slice<double> heights;
        const auto cloud = contracts::ngRigPoints(material.sourceIndexValue("cloud"));
        for (const auto vertex : cloud) heights.append(mouthFrame.local(vertex)[1]);
        basis = percentile(std::span<const double>(heights.data(), heights.size()), .5);
    }
    const double factor = x < 0 ? 1 + x * .985 : 1 + x * mouthFrame.width * .20 / height;
    double t = point[0] / (mouthFrame.width / 2);
    // Original clamp has fixed ordered bounds and preserves an unordered input.
    if (t < -1.) t = -1.;
    else if (t > 1.) t = 1.;
    return {0., (factor - 1) * basis + expression * mouthFrame.width * .035 * (1 - t * t)};
}

} // namespace nijigenerate::autorig::deterministic::controls_detail
