#pragma once
// Used std.string.toStringz out contract, The D Language Foundation 2007-.
// BSL-1.0; see port/licenses/phobos-source-c-string.txt.
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <cstring>
#include <string_view>

namespace nijigenerate::compat::detail {
inline void ngSourceCStringPostconditions(std::string_view input, const char* output) {
#ifndef NDEBUG
    if (!output) return;
    auto length = input.size();
    while (length && input[length - 1] == '\0') --length;
    if (std::strlen(output) != length)
        throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("The result c string is shorter than the in input string"),
            RetainedString::fromStaticLiteral("std/string.d"), 369);
    if (length && std::memcmp(output, input.data(), length) != 0)
        throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("The input and result string are not equal"),
            RetainedString::fromStaticLiteral("std/string.d"), 371);
#else
    (void)input;
    (void)output;
#endif
}
}
