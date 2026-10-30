#pragma once

#include "phobos_base64_exception.hpp"
#include "retained_string_slice.hpp"
#include "slice.hpp"
#include <cstdint>

namespace nijigenerate::compat::phobos {

// The original padded Base64 char-array decoder, backed by the existing kernel.
// Its defining production target is nijigenerate_autorig_native.
Slice<std::uint8_t> ngDecodeBase64(RetainedStringSlice source);

} // namespace nijigenerate::compat::phobos
