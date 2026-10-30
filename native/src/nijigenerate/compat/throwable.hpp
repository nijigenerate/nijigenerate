#pragma once
// Compatibility spelling of the one canonical SDK provider.
#include <compat/static_error.hpp>

namespace nijigenerate::compat {
using ::nicxlive::core::d_runtime::Throwable;
using ::nicxlive::core::d_runtime::ThrowableException;
using ::nicxlive::core::d_runtime::ThrowableError;
using ::nicxlive::core::d_runtime::ngStaticError;
using ::nicxlive::core::d_runtime::ngCastSourceError;
using ::nicxlive::core::d_runtime::ngRethrowSourceError;
}
