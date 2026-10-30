#pragma once
// Compatibility spelling of the single canonical SDK std.file provider.
#include "throwable.hpp"
#include "phobos_unicode_cursor.hpp"
#include <compat/phobos_file_exception.hpp>
namespace nijigenerate::compat {
using ::nicxlive::core::d_runtime::FileException;
namespace file_exception_detail = ::nicxlive::core::d_runtime::file_exception_detail;
}
