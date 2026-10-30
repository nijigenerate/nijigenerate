#pragma once

#include <common/shared_array.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <limits>
#include <optional>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>

namespace nijigenerate::widgets::inputtext::detail {

inline ::nicxlive::core::common::SharedArray<char> inputTextBuffer(const std::optional<std::string>& value) try {
    const auto length = value ? value->size() : 0;
    if (length == (std::numeric_limits<std::size_t>::max)())
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    ::nicxlive::core::common::SharedArray<char> buffer(length + 1, '\0');
    for (std::size_t i = 0; i < length; ++i) buffer[i] = (*value)[i];
    return buffer;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

inline void storeInputText(std::optional<std::string>& destination, std::span<const char> source) try {
    std::size_t length = 0;
    while (length < source.size() && source[length] != '\0') ++length;
    if (length == 0) destination.reset();
    else destination = std::string(source.data(), length);
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

inline ::nicxlive::core::common::SharedArray<char> inputTextBuffer(compat::RetainedStringSlice value) try {
    if (value.size() == (std::numeric_limits<std::size_t>::max)())
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    ::nicxlive::core::common::SharedArray<char> buffer(value.size() + 1, '\0');
    for (std::size_t i = 0; i < value.size(); ++i) buffer[i] = value.data()[i];
    return buffer;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

inline void storeInputText(compat::RetainedStringSlice& destination, std::span<const char> source) try {
    std::size_t length = 0;
    while (length < source.size() && source[length] != '\0') ++length;
    if (length == 0) destination.reset();
    else destination = compat::RetainedString(std::string(source.data(), length));
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

} // namespace nijigenerate::widgets::inputtext::detail
