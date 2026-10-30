#pragma once
#include "nijigenerate/compat/phobos_index_sort.hpp"

namespace nijigenerate::autorig::deterministic::geometry::detail {
template<class Less> void guideSortOrder(std::span<std::size_t> order, Less less) {
    compat::phobos_index_sort::sort<sizeof(std::size_t)>(order, less,
        sizeof(std::size_t) == 8 ? "Failed to sort range of type ulong[]" : "Failed to sort range of type uint[]");
}
}
