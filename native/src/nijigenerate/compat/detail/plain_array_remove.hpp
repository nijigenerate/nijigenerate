#pragma once

#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <cstddef>

namespace nijigenerate::compat::detail {

// Scalar stable-remove lowering from Phobos; see port/licenses/phobos-depth-rig-remove.txt.
// Used only for real array descriptors whose Source elements have no destructor or postblit.
template<class Array> Array ngRemovePlainArrayStable(Array values, std::size_t index) {
#ifndef NDEBUG
    if (index > values.size()) throw ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
        RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
    if (index > values.size()) throw ngStaticError<ArraySliceError>(
        index, values.size(), values.size(),
        RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
    auto source = values.slice(index, values.size());
    auto destination = source;
#ifndef NDEBUG
    if (source.empty()) throw ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
        RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
    if (source.empty()) throw ngStaticError<ArraySliceError>(
        1, source.size(), source.size(),
        RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
    source = source.slice(1, source.size());
    const auto result = values.slice(0, values.size() - 1);
    // D move copies these plain Source elements. C++ moves would clear shared
    // handles in the old trailing view, which remains observable through aliases.
    for (std::size_t i = 0; i < source.size(); ++i) destination[i] = source[i];
    return result;
}

} // namespace nijigenerate::compat::detail
