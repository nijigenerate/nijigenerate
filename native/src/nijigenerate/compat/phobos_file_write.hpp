#pragma once
// Compatibility spelling of the one canonical Source std.file.write producer.
#include "retained_string_slice.hpp"
#include "phobos_file_exception.hpp"
#include "phobos_temp_cstring.hpp"
#include <compat/phobos_file_write.hpp>
namespace nijigenerate::compat::phobos {
inline void ngFileWrite(RetainedStringSlice name, std::string_view buffer) {
    ::nicxlive::core::d_runtime::phobos::ngFileWrite(name.retainedString(), buffer);
}
}
