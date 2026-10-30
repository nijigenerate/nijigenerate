#pragma once

// Adapted from installed LDC 1.41 Phobos std.conv and std.utf.
// Copyright The D Language Foundation 2007- (std.conv), 2000-2012 (std.utf).
// Boost Software License 1.0; see phobos.LICENSE in this directory.
#include "../config_errors.hpp"
#include <limits>

namespace njc::config::detail {

inline std::size_t ngInvalidSequenceLength(std::string_view suffix) {
    std::size_t length = 0;
    if (!suffix.empty()) {
        do { ++length; }
        while (length < suffix.size() && length < 4 &&
            (static_cast<unsigned char>(suffix[length]) & 0xc0) == 0x80);
    }
    return length;
}

// Only the first scalar is decoded: a later invalid suffix does not mask this error.
inline std::size_t ngConversionCharacterLength(std::string_view suffix) {
    auto first = static_cast<std::uint8_t>(suffix.front());
    if (first < 0x80) return 1;
    if ((first & 0xc0) != 0xc0) throw ConversionUtfException(suffix, false);
    std::uint32_t scalar = first;
    first = static_cast<std::uint8_t>(first << 1);
    constexpr std::uint32_t masks[]{0x7f, 0x7ff, 0xffff, 0x1fffff};
    for (std::size_t i = 1; i <= 3; ++i) {
        if (i == suffix.size()) throw ConversionUtfException(suffix, true);
        const auto byte = static_cast<std::uint8_t>(suffix[i]);
        if ((byte & 0xc0) != 0x80) throw ConversionUtfException(suffix, false);
        scalar = (scalar << 6) | (byte & 0x3f);
        first = static_cast<std::uint8_t>(first << 1);
        if ((first & 0x80) == 0) {
            scalar &= masks[i];
            if ((scalar & ~masks[i - 1]) == 0 ||
                (i == 2 && scalar >= 0xd800 && scalar <= 0xdfff) ||
                (i == 3 && scalar > 0x10ffff)) throw ConversionUtfException(suffix, false);
            return i + 1;
        }
    }
    throw ConversionUtfException(suffix, false);
}

[[noreturn]] inline void ngThrowConversionError(std::string_view suffix) {
    if (suffix.empty())
        throw ConversionException(nijigenerate::compat::RetainedString::fromStaticLiteral(
            "Unexpected end of input when converting from type string to type int"));
    const auto length = ngConversionCharacterLength(suffix);
    auto message = ngConfigAllocation([&] {
        const auto shown = suffix.front() == '\n' ? std::string("\\n") : std::string(suffix.substr(0, length));
        return std::string("Unexpected '") + shown + "' when converting from type string to type int";
    });
    throw ConversionException(std::move(message));
}

// std.conv.to!int(string): signed decimal parse followed by the whole-input check.
inline std::int32_t ngPortInteger(std::string_view source) {
    if (source.empty()) ngThrowConversionError(source);
    bool negative = false;
    if (source.front() == '-' || source.front() == '+') {
        negative = source.front() == '-';
        source.remove_prefix(1);
    }
    if (source.empty() || source.front() < '0' || source.front() > '9') ngThrowConversionError(source);
    const auto limit = std::uint32_t(std::numeric_limits<std::int32_t>::max()) + (negative ? 1U : 0U);
    std::uint32_t magnitude = 0;
    std::size_t consumed = 0;
    while (consumed < source.size()) {
        const auto byte = static_cast<unsigned char>(source[consumed]);
        if (byte < '0' || byte > '9') break;
        const auto digit = std::uint32_t(byte - '0');
        if (magnitude > limit / 10 || (magnitude == limit / 10 && digit > limit % 10))
            throw ConversionOverflowException(nijigenerate::compat::RetainedString::fromStaticLiteral(
                "Overflow in integral conversion"));
        magnitude = magnitude * 10 + digit;
        ++consumed;
    }
    if (consumed != source.size()) ngThrowConversionError(source.substr(consumed));
    if (negative && magnitude == limit) return std::numeric_limits<std::int32_t>::min();
    const auto value = static_cast<std::int32_t>(magnitude);
    return negative ? -value : value;
}

} // namespace njc::config::detail
