#pragma once

#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

// The original nested advanced body, consumed directly by the GridAutoMesh stage.
// Preserve its unguarded narrowing; configured D/native FP execution is unverified.
compat::Slice<float> advanced(compat::Slice<double> desired, compat::Slice<double> bounds, bool headSurface = false);

} // namespace nijigenerate::autorig::deterministic::native_detail
