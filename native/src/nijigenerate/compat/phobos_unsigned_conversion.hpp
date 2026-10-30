#pragma once

// Adapted from independent LDC 1.41 Phobos std.conv/std.utf.
// Copyright The D Language Foundation 2007- (conv), 2000-2012 (utf).
// Boost Software License 1.0: native/port/licenses/phobos-unicode.txt.
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::compat::phobos {

class ConvException : public ::nijigenerate::compat::ThrowableException {
public:
    explicit ConvException(std::string message)
        : ThrowableException(::nijigenerate::compat::RetainedString(std::move(message))) {}
    ConvException(::nijigenerate::compat::RetainedString message,
        ::nijigenerate::compat::RetainedString sourceFile, std::size_t sourceLine)
        : ThrowableException(std::move(message), std::move(sourceFile), sourceLine) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.conv.ConvException"; }
};
class ConvOverflowException : public ConvException {
public:
    ConvOverflowException() : ConvException("Overflow in integral conversion") {}
    ConvOverflowException(::nijigenerate::compat::RetainedString message,
        ::nijigenerate::compat::RetainedString sourceFile, std::size_t sourceLine)
        : ConvException(std::move(message), std::move(sourceFile), sourceLine) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.conv.ConvOverflowException"; }
};
inline std::size_t ngUnexpectedScalarLength(std::string_view suffix) {
    using Cursor = ::nijigenerate::compat::phobos::detail::UnicodeCursor<std::string_view, char>;
    // Decode only the unexpected first scalar while the caller's string is alive.
    const auto scalar = Cursor(suffix).front();
    return scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
}

[[noreturn]] inline void ngThrowUnsignedConversion(std::string_view suffix) {
    if (suffix.empty())
        throw ConvException("Unexpected end of input when converting from type string to type ulong");
    const auto length = ngUnexpectedScalarLength(suffix);
    const auto shown = suffix.front() == '\n' ? std::string("\\n") : std::string(suffix.substr(0, length));
    throw ConvException("Unexpected '" + shown + "' when converting from type string to type ulong");
}

inline std::uint64_t ngUnsignedDecimal64(std::string_view source) {
    if (source.empty()) ngThrowUnsignedConversion(source);
    std::uint64_t result = 0;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto byte = static_cast<unsigned char>(source[i]);
        if (byte < '0' || byte > '9') ngThrowUnsignedConversion(source.substr(i));
        const auto digit = static_cast<unsigned>(byte - '0');
        if (result > maximum / 10 || (result == maximum / 10 && digit > maximum % 10))
            throw ConvOverflowException();
        result = result * 10 + digit;
    }
    return result;
}

} // namespace nijigenerate::compat::phobos
