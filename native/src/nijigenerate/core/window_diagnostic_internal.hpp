#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::core::detail {
inline compat::RetainedString retainWindowText(std::string_view prefix, std::string_view suffix = {}) {
    try {
        std::string bytes(prefix);
        if (!suffix.empty()) bytes.append(suffix.data(), suffix.size());
        return compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
// Synchronous source writefln boundary; views borrow the actual queried GL strings.
void writeWindowGLInformation(const std::array<std::string_view, 4>& values, std::uint32_t contextVersion);
}
