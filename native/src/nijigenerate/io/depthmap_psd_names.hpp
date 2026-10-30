#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <cstdint>

namespace nijigenerate::io::depthmap_psd {

compat::RetainedString ngPsdDepthLowerString(compat::RetainedString text);

compat::RetainedStringSlice ngPsdDepthNormalizeLayerName(compat::RetainedStringSlice name);
compat::RetainedStringSlice ngPsdDepthLayerIdentityKey(compat::RetainedStringSlice name,
    std::int32_t left, std::int32_t top, std::int32_t width, std::int32_t height);

} // namespace nijigenerate::io::depthmap_psd
