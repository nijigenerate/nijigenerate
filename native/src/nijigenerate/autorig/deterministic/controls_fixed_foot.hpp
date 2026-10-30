#pragma once

#include "controls_frame.hpp"
#include <array>
#include <limits>

namespace nijigenerate::autorig::deterministic::controls_detail {

// Stores the actual original locals before the checkpoint/key loop, without running that loop.
struct FixedFootTargetInput {
    compat::RetainedStringSlice side;
    std::array<contracts::Point2, 3> joints;
    contracts::Point2 axis;
    contracts::Point3 stations;
    compat::Slice<double> xs, ys, matrix;
    double determinant = std::numeric_limits<double>::quiet_NaN();
};

FixedFootTargetInput prepareFixedFootTarget(core::settings::SettingsJsonValue target,
    core::settings::SettingsJsonValue landmarks);

// Actual per-key body with explicit lexical captures, after the original checkpoint and angle calculation.
// The enclosing public fixed-foot compiler and its providers remain incomplete.
core::settings::SettingsJsonValue fixedFootKey(core::settings::SettingsJsonValue target,
    std::size_t index, double angle, std::array<contracts::Point2, 3> joints,
    contracts::Point2 pelvis, contracts::Point2 axis, contracts::Point3 stations,
    compat::Slice<double> xs, compat::Slice<double> ys, compat::Slice<double> matrix,
    double determinant, compat::RetainedStringSlice side);

} // namespace nijigenerate::autorig::deterministic::controls_detail
