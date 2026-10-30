#pragma once
// The single canonical Source C-string producer is shared with the SDK.
#include "out_of_memory_error.hpp"
#include <compat/phobos_temp_cstring.hpp>
namespace nijigenerate::compat::phobos {
using ::nicxlive::core::d_runtime::phobos::TempCStringBuffer;
}
