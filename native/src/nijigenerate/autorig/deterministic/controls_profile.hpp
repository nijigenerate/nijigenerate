#pragma once

#include "controls_frame.hpp"

namespace nijigenerate::autorig::deterministic::controls_detail {

Profile profile(compat::Slice<contracts::Point2> support, Frame frame);

} // namespace nijigenerate::autorig::deterministic::controls_detail
