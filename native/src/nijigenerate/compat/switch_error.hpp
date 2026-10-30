#pragma once
#include "throwable.hpp"
// Compatibility spelling of the canonical shared SDK source-error provider.
#include <compat/switch_error.hpp>
namespace nijigenerate::compat {
using ::nicxlive::core::d_runtime::SwitchError;
using ::nicxlive::core::d_runtime::ngStaticError;
}
