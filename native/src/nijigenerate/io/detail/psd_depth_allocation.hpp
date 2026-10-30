#pragma once

#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nijigenerate::io::depthmap_psd::detail {
template<class Operation> decltype(auto) allocateImportBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::RetainedString retainImportUnsignedDecimal(std::uint64_t value) {
    char digits[20];
    const auto converted = std::to_chars(digits, digits + sizeof(digits), value);
    return allocateImportBytes([&] {
        return compat::RetainedString(std::string_view(digits,
            static_cast<std::size_t>(converted.ptr - digits)));
    });
}
}
