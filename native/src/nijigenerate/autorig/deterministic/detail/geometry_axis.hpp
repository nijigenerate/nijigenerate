#pragma once
#include "nijigenerate/compat/slice.hpp"

namespace nijigenerate::autorig::deterministic::geometry::detail {
// Internal module helper, not part of the public geometry facade.
void lines(compat::Slice<double> axis);
}
