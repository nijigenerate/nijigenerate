#pragma once
// Current independent Phobos std.conv exception shape; BSL-1.0 notice:
// native/port/licenses/phobos-videoexport.txt.

#include "phobos_unsigned_conversion.hpp"

namespace nijigenerate::compat {
using PhobosConvException = phobos::ConvException;
using PhobosConvOverflowException = phobos::ConvOverflowException;
} // namespace nijigenerate::compat
