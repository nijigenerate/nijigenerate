#pragma once

#include "array_range_error.hpp"

namespace nijigenerate::compat {

template<class Array> decltype(auto) ngArrayAt(Array& values, std::size_t index,
    std::source_location source = std::source_location::current()) {
    const auto length = values.size();
    if (index >= length) throw ngStaticError<ArrayIndexError>(index, length, source);
    return values[index];
}

template<class Array> auto ngArraySlice(const Array& values, std::size_t lower, std::size_t upper,
    std::source_location source = std::source_location::current()) {
    const auto length = values.size();
    if (lower > upper || upper > length) throw ngStaticError<ArraySliceError>(lower, upper, length, source);
    return values.slice(lower, upper);
}

} // namespace nijigenerate::compat
