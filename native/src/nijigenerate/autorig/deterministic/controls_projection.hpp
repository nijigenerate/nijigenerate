#pragma once

#include "controls_skin.hpp"

namespace nijigenerate::autorig::deterministic::controls_detail {

// These are the two lexical arithmetic bodies, with their actual captures passed explicitly.
// The enclosing cheek compiler and its context/JSON/bool integration remain unavailable.
contracts::Point2 gridLocal(contracts::Point2 point, const compat::Slice<double>& matrix,
    double determinant);
contracts::Point2 projected(contracts::Point2 section, const SkinProfile& profile,
    const compat::Slice<double>& matrix, double determinant,
    compat::Slice<contracts::Point2> offsets, compat::Slice<double> xs, compat::Slice<double> ys);

} // namespace nijigenerate::autorig::deterministic::controls_detail
