#pragma once
// Phobos std.string.toStringz body. Copyright The D Language Foundation 2007-.
// BSL-1.0; see port/licenses/phobos-source-c-string.txt.

#include "retained_string_slice.hpp"
#include <compat/source_c_string.hpp>
#include "slice.hpp"
#include "out_of_memory_error.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace nijigenerate::compat {

// Preserve the existing accepted App runtime source origin and public intakes.
class SourceCString : public ::nicxlive::core::d_runtime::SourceCString<519> {
    using Base = ::nicxlive::core::d_runtime::SourceCString<519>;
public:
    explicit SourceCString(RetainedStringSlice input) : Base(input.view()) {}
    explicit SourceCString(std::string_view input) : Base(input) {}
};
}
