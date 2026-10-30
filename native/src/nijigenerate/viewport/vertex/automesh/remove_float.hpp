#pragma once

// Phobos std.algorithm.mutation, Copyright Andrei Alexandrescu 2008-.
// Boost Software License 1.0; see native/port/licenses/phobos-unicode.txt.
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::viewport::vertex::automesh::detail {

// Actual single-offset float[] specialization of Phobos removeStable.
// Keep the live backing and shorten only the returned descriptor.
inline compat::Slice<float> removeFloat(compat::Slice<float> values, std::size_t index) {
    auto result = values;
    auto source = values;
    auto target = values;
    const auto popFrontExactly = [](compat::Slice<float>& range, std::size_t count) {
#ifndef NDEBUG
        if (count > range.size())
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
                compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
        if (count > range.size())
            throw compat::ngStaticError<compat::ArraySliceError>(count, range.size(), range.size(),
                compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
        range = range.slice(count, range.size());
    };
    popFrontExactly(source, index);
    popFrontExactly(target, index);
    popFrontExactly(source, 1);
    result = result.slice(0, result.size() - 1);
    for (std::size_t i = 0; i < source.size(); ++i) target[i] = source[i];
    return result;
}

} // namespace nijigenerate::viewport::vertex::automesh::detail
