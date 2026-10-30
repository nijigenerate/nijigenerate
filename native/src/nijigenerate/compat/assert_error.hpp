#pragma once
// Compatibility spelling of the one canonical SDK provider.
#include <compat/assert_error.hpp>

namespace nijigenerate::compat {
using ::nicxlive::core::d_runtime::AssertError;
using ::nicxlive::core::d_runtime::ngAssertUnreachable;
using ::nicxlive::core::d_runtime::ngStaticError;
}
