#include "anatomy.hpp"
#include <cmath>

namespace nijigenerate::autorig::deterministic::anatomy::detail {

double length(contracts::Point2 point) {
    return std::sqrt(point[0] * point[0] + point[1] * point[1]);
}

contracts::Point2 subtract(contracts::Point2 left, contracts::Point2 right) {
    return {left[0] - right[0], left[1] - right[1]};
}

double dot(contracts::Point2 left, contracts::Point2 right) {
    return left[0] * right[0] + left[1] * right[1];
}

} // namespace nijigenerate::autorig::deterministic::anatomy::detail
