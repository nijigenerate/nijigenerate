#pragma once

// Compatibility adaptation of Phobos std.conv.parse!uint and convError.
// Copyright The D Language Foundation 2007-.
// Distributed under Boost Software License 1.0; see phobos_sort.LICENSE.

#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cstdint>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::core::selector {

using SelectorConvException = compat::PhobosConvException;
using SelectorConvOverflowException = compat::PhobosConvOverflowException;

namespace detail {
inline compat::RetainedString ngSelectorUIntUnexpectedMessage(std::string_view source) {
    // Source convError reads source.front before formatting its owned message.
    // Keep the real Unicode decoder and its failures outside this boundary.
    const auto count = compat::phobos::ngUnexpectedScalarLength(source);
    try {
        std::string message = "Unexpected '";
        if (source.front() == '\n') message += "\\n";
        else message.append(source.substr(0, count));
        message += "' when converting from type string to type uint";
        return compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

// D parse!uint(ref string): decimal prefix only, committing the slice only on success.
inline std::uint32_t ngParseSelectorUInt(std::string_view& source) {
    if (source.empty()) {
        throw SelectorConvException(compat::RetainedString::fromStaticLiteral(
            "Unexpected end of input when converting from type string to type uint"),
            compat::RetainedString::fromStaticLiteral("std/conv.d"), 2547);
    }
    const auto first = static_cast<unsigned char>(source.front());
    if (first < '0' || first > '9') {
        const auto message = detail::ngSelectorUIntUnexpectedMessage(source);
        throw SelectorConvException(message, compat::RetainedString::fromStaticLiteral("std/conv.d"), 2547);
    }
    std::uint32_t value = 0;
    std::size_t consumed = 0;
    for (const auto character : source) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < '0' || byte > '9') break;
        const auto digit = static_cast<std::uint32_t>(byte - '0');
        if (value > std::numeric_limits<std::uint32_t>::max() / 10 ||
            (value == std::numeric_limits<std::uint32_t>::max() / 10 && digit > 5)) {
            throw SelectorConvOverflowException(compat::RetainedString::fromStaticLiteral(
                "Overflow in integral conversion"), compat::RetainedString::fromStaticLiteral("std/conv.d"), 2527);
        }
        value = value * 10 + digit;
        ++consumed;
    }
    source.remove_prefix(consumed);
    return value;
}
}
