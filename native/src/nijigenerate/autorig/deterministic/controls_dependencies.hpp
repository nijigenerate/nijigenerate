#pragma once

#include "contracts.hpp"
#include "observation.hpp"
#include <nijigenerate/autorig/framework.hpp>

namespace nijigenerate::autorig::deterministic::controls_detail {
// Original private least-squares endpoint-preserving fit.
compat::Slice<double> endpointCubic(compat::Slice<double> values);
}
