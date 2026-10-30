#pragma once

// Adapted from independent Phobos std.format.internal.write (D Language
// Foundation, Boost 1.0). The included layout header retains the full license.
#include "typed_format_graphical.hpp"
#include <nijigenerate/compat/utf8.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <charconv>

namespace nijigenerate::core::translation::detail {

template<class Build> decltype(auto) allocateQuotedBytes(Build&& build) {
    try { return std::forward<Build>(build)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

// Element quoting follows independent Phobos formatChar/formatElement. The
// Graphical set is generated from that same installed source, not a platform
// locale, ASCII approximation or unrelated Unicode database version.
inline std::string formatQuotedCodepoint(char32_t value, char quote) {
    if (isFormatGraphical(value)) {
        auto result = formatSeparatorUtf8(value);
        if (value == static_cast<char32_t>(quote) || value == '\\')
            allocateQuotedBytes([&] { result.insert(result.begin(), '\\'); });
        return result;
    }
    constexpr char controls[] = {'\n', '\r', '\t', '\a', '\b', '\f', '\v', '\0'};
    constexpr char escapes[] = {'n', 'r', 't', 'a', 'b', 'f', 'v', '0'};
    for (std::size_t i = 0; i < sizeof(controls); ++i) {
        if (value == static_cast<unsigned char>(controls[i]))
            return allocateQuotedBytes([&] { return std::string{'\\', escapes[i]}; });
    }
    const auto width = value <= 0xff ? 2u : value <= 0xffff ? 4u : 8u;
    std::string result = allocateQuotedBytes([&] {
        return std::string(width == 2 ? "\\x" : width == 4 ? "\\u" : "\\U");
    });
    for (auto shift = width * 4; shift != 0; shift -= 4) {
        const auto digit = static_cast<unsigned>((value >> (shift - 4)) & 15);
        allocateQuotedBytes([&] {
            result.push_back(static_cast<char>(digit < 10 ? '0' + digit : 'A' + digit - 10));
        });
    }
    return result;
}

inline std::string formatQuotedString(std::string_view text) {
    std::string result;
    try {
        using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view,char>;
        // Validate before output, then decode the same descriptor again while emitting.
        Cursor validation(text);
        bool valid = true;
        while (!validation.empty()) {
            const auto scalar = validation.front();
            validation.popFront();
            if (scalar == 0xfffe || scalar == 0xffff) {
                valid = false;
                break;
            }
        }
        if (valid) {
            allocateQuotedBytes([&] { result += '"'; });
            Cursor emission(text);
            while (!emission.empty()) {
                const auto scalar = emission.front();
                emission.popFront();
                const auto escaped = formatQuotedCodepoint(scalar, '"');
                allocateQuotedBytes([&] { result += escaped; });
            }
            allocateQuotedBytes([&] { result += '"'; });
            return result;
        }
    } catch (const compat::UTFException&) {}
    // Invalid UTF-8 is represented as the actual original unsigned code units,
    // preserving every byte rather than replacing a bad sequence by U+FFFD.
    allocateQuotedBytes([&] { result += '['; });
    bool first = true;
    for (unsigned char byte : text) {
        if (!first) allocateQuotedBytes([&] { result += ", "; });
        first = false;
        allocateQuotedBytes([&] { result += "cast(char) 0x"; });
        char digits[2];
        const auto converted = std::to_chars(digits, digits + sizeof(digits), byte, 16);
        for (auto p = digits; p != converted.ptr; ++p)
            allocateQuotedBytes([&] {
                result.push_back(*p >= 'a' && *p <= 'f' ? static_cast<char>(*p - 'a' + 'A') : *p);
            });
    }
    allocateQuotedBytes([&] { result += ']'; });
    return result;
}

} // namespace nijigenerate::core::translation::detail
