#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <cstdint>

namespace nijigenerate::io::imageexport {

// Public image export entry; the ordinary imageexport target supplies its body.
void incExportImage(compat::RetainedStringSlice file, compat::Slice<std::uint8_t> data,
    std::int32_t width, std::int32_t height, std::int32_t channels = 0);

} // namespace nijigenerate::io::imageexport
