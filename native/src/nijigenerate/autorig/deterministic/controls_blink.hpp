#pragma once

#include "controls_frame.hpp"
#include <string_view>

namespace nijigenerate::autorig::deterministic::controls_detail {

// Actual blink arithmetic with explicit live captures; this is not an integrated mechanism delegate.
contracts::Point2 blinkDisplacement(std::string_view kind, contracts::Point2 point,
    double blink, double expression, const Frame& eye, Profile& aperture,
    const compat::Slice<double>& flatClose, const compat::Slice<double>& smileClose,
    const compat::Slice<double>& expressionOffset, const compat::Slice<double>& upperContact,
    const compat::Slice<double>& lowerContact);

} // namespace nijigenerate::autorig::deterministic::controls_detail
