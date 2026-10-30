#pragma once

#include <nijigenerate/compat/assert_error.hpp>

namespace nijigenerate::actions::depthboneinvalidation {

// Use the same assertion carrier as the existing source-range implementations.
// Its native runtime/Throwable integration is a shared provider obligation.
using DepthBoneMutationAssertion = compat::AssertError;

} // namespace nijigenerate::actions::depthboneinvalidation
