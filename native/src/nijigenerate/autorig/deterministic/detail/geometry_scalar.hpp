#pragma once
#include "nijigenerate/autorig/deterministic/contracts.hpp"

namespace nijigenerate::autorig::deterministic::geometry::detail {
using contracts::Point2;
double cross(Point2 a, Point2 b);
Point2 sub(Point2 a, Point2 b);
double squared(Point2 a);
Point2 guideBase(Point2 uv, const compat::FixedArray<double, 4>& bounds);
}
