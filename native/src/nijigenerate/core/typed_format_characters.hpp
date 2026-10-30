#pragma once

#include "typed_format_quote.hpp"
#include <span>

namespace nijigenerate::core::translation::detail {

inline char32_t validateFormatDchar(char32_t scalar) {
    if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) {
        compat::UTFException error(compat::RetainedString::fromStaticLiteral("Invalid UTF-32 value"),
            compat::RetainedString::fromStaticLiteral("std/utf.d"), 1869);
        const std::array<std::uint32_t, 1> sequence{scalar};
        error.setSequence(sequence);
        throw error;
    }
    return scalar;
}

inline std::vector<char32_t> decodeFormatCodeUnits(std::span<const char32_t> units, unsigned bits,
    bool validateDchars = true) {
    std::vector<char32_t> result;
    if (bits == 8) {
        std::string bytes;
        allocateQuotedBytes([&] {
            bytes.reserve(units.size());
            for (auto unit : units) bytes.push_back(static_cast<char>(unit));
        });
        compat::phobos::detail::UnicodeCursor<std::string_view,char> cursor(bytes);
        while (!cursor.empty()) {
            const auto scalar = cursor.front();
            allocateQuotedBytes([&] { result.push_back(scalar); });
            cursor.popFront();
        }
        return result;
    }
    if (bits != 16 && bits != 32) throw std::invalid_argument("Unsupported character code-unit width");
    if (bits==16) {
        compat::phobos::detail::UnicodeCursor<std::span<const char32_t>,char16_t> cursor(units);
        while (!cursor.empty()) {
            const auto scalar = cursor.front();
            allocateQuotedBytes([&] { result.push_back(scalar); });
            cursor.popFront();
        }
        return result;
    }
    for (auto scalar:units) {
        if (validateDchars) (void)validateFormatDchar(scalar);
        allocateQuotedBytes([&] { result.push_back(scalar); });
    }
    return result;
}

inline std::string encodeFormatCodeUnits(std::span<const char32_t> units, unsigned bits) {
    std::string result;
    if (bits == 8) {
        allocateQuotedBytes([&] {
            for (auto unit : units) result.push_back(static_cast<char>(unit));
        });
    } else {
        for (auto scalar : decodeFormatCodeUnits(units, bits)) {
            const auto encoded = formatSeparatorUtf8(scalar);
            allocateQuotedBytes([&] { result += encoded; });
        }
    }
    return result;
}

inline std::string formatQuotedCodeUnits(std::span<const char32_t> units, unsigned bits) {
    if (bits == 8) return formatQuotedString(encodeFormatCodeUnits(units, bits));
    if (bits != 16 && bits != 32) throw std::invalid_argument("Unsupported character code-unit width");
    const auto forEachScalar = [&](auto&& consume) {
        if (bits == 16) {
            compat::phobos::detail::UnicodeCursor<std::span<const char32_t>,char16_t> cursor(units);
            while (!cursor.empty()) {
                const auto scalar = cursor.front();
                cursor.popFront();
                if (!consume(scalar)) return false;
            }
        } else {
            for (auto unit : units) {
                const auto scalar = validateFormatDchar(unit);
                if (!consume(scalar)) return false;
            }
        }
        return true;
    };
    std::string result;
    try {
        const bool valid = forEachScalar([](char32_t scalar) { return scalar != 0xfffe && scalar != 0xffff; });
        if (valid) {
            allocateQuotedBytes([&] { result += '"'; });
            forEachScalar([&](char32_t scalar) {
                const auto escaped = formatQuotedCodepoint(scalar, '"');
                allocateQuotedBytes([&] { result += escaped; });
                return true;
            });
            allocateQuotedBytes([&] { result += '"'; });
            return result;
        }
    } catch (const compat::UTFException&) {}
    allocateQuotedBytes([&] { result += '['; });
    for (std::size_t i = 0; i < units.size(); ++i) {
        allocateQuotedBytes([&] {
            if (i) result += ", ";
            result += bits == 16 ? "cast(wchar) 0x" : "cast(dchar) 0x";
        });
        char digits[8];
        const auto converted = std::to_chars(digits, digits + sizeof(digits),
            static_cast<std::uint32_t>(units[i]), 16);
        for (auto p = digits; p != converted.ptr; ++p)
            allocateQuotedBytes([&] {
                result.push_back(*p >= 'a' && *p <= 'f' ? static_cast<char>(*p - 'a' + 'A') : *p);
            });
    }
    allocateQuotedBytes([&] { result.push_back(']'); });
    return result;
}

} // namespace nijigenerate::core::translation::detail
