#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <common/string_key_map.hpp>
#include <cstddef>

namespace nijigenerate::io::depthmap_psd::detail {

using PsdLayerPathOccurrences = ::nicxlive::core::common::StringKeyMap<std::size_t>;

compat::RetainedString uniquePsdLayerPath(compat::RetainedString path, PsdLayerPathOccurrences& occurrences);

}
