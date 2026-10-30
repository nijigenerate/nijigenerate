#include "controls_projection.hpp"

namespace nijigenerate::autorig::deterministic::controls_detail {

contracts::Point2 gridLocal(contracts::Point2 point, const compat::Slice<double>& matrix,
    double determinant) {
    const double xFirstOffset = point[0] - matrix[2];
    const double xFirstProduct = xFirstOffset * matrix[4];
    const double xSecondOffset = point[1] - matrix[5];
    const double xSecondProduct = xSecondOffset * matrix[1];
    const double x = (xFirstProduct - xSecondProduct) / determinant;
    const double yFirstScale = matrix[0];
    const double yFirstOffset = point[1] - matrix[5];
    const double yFirstProduct = yFirstScale * yFirstOffset;
    const double ySecondScale = matrix[3];
    const double ySecondOffset = point[0] - matrix[2];
    const double ySecondProduct = ySecondScale * ySecondOffset;
    return {x, (yFirstProduct - ySecondProduct) / determinant};
}

contracts::Point2 projected(contracts::Point2 section, const SkinProfile& profile,
    const compat::Slice<double>& matrix, double determinant,
    compat::Slice<contracts::Point2> offsets, compat::Slice<double> xs, compat::Slice<double> ys) {
    const contracts::Point2 world = {
        profile.frame.origin[0] + section[0] * profile.frame.tangent[0] +
            section[1] * profile.frame.normal[0],
        profile.frame.origin[1] + section[0] * profile.frame.tangent[1] +
            section[1] * profile.frame.normal[1]};
    const auto delta = gridSample(offsets, xs, ys, gridLocal(world, matrix, determinant));
    return {world[0] + matrix[0] * delta[0] + matrix[1] * delta[1],
        world[1] + matrix[3] * delta[0] + matrix[4] * delta[1]};
}

} // namespace nijigenerate::autorig::deterministic::controls_detail
