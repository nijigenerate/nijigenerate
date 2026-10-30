#pragma once

#include "controls_frame.hpp"

namespace nijigenerate::autorig::deterministic::controls_detail {

// Actual lexical arithmetic with explicit captures; mechanism/delegate integration is still gated.
contracts::Point2 localBendDisplacement(contracts::Point2 point, double x, double y,
    const Frame& localFrame);
contracts::Point2 irisDisplacement(double x, double y, const Frame& eye, double height);
contracts::Point2 eyebrowDisplacement(contracts::Point2 point, double x, double y, const Frame& eye);
// rigid is the result of the original material-name classification, not a substitute classifier.
contracts::Point2 mouthDisplacement(contracts::Point2 point, double x, double expression,
    core::settings::SettingsJsonValue material, bool rigid, Frame& mouthFrame, double height);

} // namespace nijigenerate::autorig::deterministic::controls_detail
