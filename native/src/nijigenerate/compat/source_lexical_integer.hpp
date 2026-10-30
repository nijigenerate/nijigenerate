#pragma once

// Adapted from LDC 1.41 dmd/lexer.d number(), D mode and single-digit dispatch.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_trivia.hpp"
#include <limits>

namespace nijigenerate::compat {

enum class SourceIntegerKind { NotInteger, RequiresReal, Int32, UInt32, Int64, UInt64 };
enum class SourceIntegerIssue : std::uint32_t {
    None = 0, InvalidDigit = 1, Overflow = 2, MissingRadixDigits = 4,
    LowercaseLongSuffix = 8, RepeatedSuffix = 16, UnsupportedOctal = 32, SignedOverflow = 64
};
struct SourceIntegerLiteral {
    SourceIntegerKind kind = SourceIntegerKind::NotInteger;
    std::uint64_t value = 0;
    SourceLexicalTrivia lexical;
    std::size_t start = 0;
    std::size_t length = 0;
    std::size_t suffixStart = 0;
    std::size_t suffixLength = 0;
    std::size_t repeatedSuffixOffset = 0;
    std::uint32_t issues = 0;
    unsigned base = 10;
    unsigned flags = 0;
    unsigned char invalidDigit = 0;
    unsigned char repeatedSuffix = 0;
    bool overflowed = false;
};
constexpr bool ngSourceIntegerHasIssue(const SourceIntegerLiteral& value, SourceIntegerIssue issue) noexcept {
    return (value.issues & static_cast<std::uint32_t>(issue)) != 0;
}

namespace detail {
constexpr bool sourceNumberAlphabetic(unsigned char byte) noexcept {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z');
}
constexpr bool sourceNumberSecond(unsigned char byte, bool zero) noexcept {
    return (byte >= '0' && byte <= '9') || byte == 'e' || byte == 'E' || byte == 'f' || byte == 'F' ||
        byte == 'l' || byte == 'L' || byte == 'p' || byte == 'P' || byte == 'u' || byte == 'U' ||
        byte == 'i' || byte == '.' || byte == '_' || (zero && (byte == 'x' || byte == 'X' || byte == 'b' || byte == 'B'));
}
}

// A real/imaginary prefix returns RequiresReal at the ORIGINAL token start.
// Its value is not an integer token and must be supplied by the real-literal
// producer. No host floating conversion or fabricated zero closes that branch.
constexpr SourceIntegerLiteral ngScanSourceIntegerLiteral(
        std::string_view source, SourceLexicalPosition position = {}) noexcept {
    SourceIntegerLiteral result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    auto& cursor = result.lexical.position;
    result.start = cursor.offset;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endOfInput) return result;
    const auto byteAt = [&](std::size_t at) -> unsigned char {
        return at < source.size() ? static_cast<unsigned char>(source[at]) : 0;
    };
    const auto requiresReal = [&] {
        result.kind = SourceIntegerKind::RequiresReal;
        cursor.offset = result.start;
        return result;
    };
    const auto first = byteAt(cursor.offset);
    if (first == '.' && byteAt(cursor.offset + 1) >= '0' && byteAt(cursor.offset + 1) <= '9') return requiresReal();
    if (first < '0' || first > '9') return result;
    if (!detail::sourceNumberSecond(byteAt(cursor.offset + 1), first == '0')) {
        result.kind = SourceIntegerKind::Int32;
        result.value = first - '0';
        ++cursor.offset;
        result.length = 1;
        return result;
    }
    const auto diagnose = [&](SourceIntegerIssue issue) { result.issues |= static_cast<std::uint32_t>(issue); };
    std::uint64_t value = 0;
    bool anyRadixDigit = false;
    bool done = false;
    if (first == '0') {
        ++cursor.offset;
        const auto byte = byteAt(cursor.offset);
        if (byte >= '0' && byte <= '9') {
            result.base = 8;
            if (byte >= '8') result.invalidDigit = byte;
        } else if (byte == 'x' || byte == 'X') { ++cursor.offset; result.base = 16; }
        else if (byte == 'b' || byte == 'B') { ++cursor.offset; result.base = 2; }
        else if (byte == '.') {
            const auto next = byteAt(cursor.offset + 1);
            if (next == '.' || detail::sourceNumberAlphabetic(next) || next == '_' || next >= 0x80) done = true;
            else return requiresReal();
        } else if (byte == 'i' || byte == 'f' || byte == 'F' || (byte == 'L' && byteAt(cursor.offset + 1) == 'i'))
            return requiresReal();
        else if (byte == '_') { ++cursor.offset; result.base = 8; }
    }
    while (!done) {
        const auto byte = byteAt(cursor.offset);
        unsigned digit = 0;
        if (byte >= '0' && byte <= '9') { digit = byte - '0'; ++cursor.offset; }
        else if ((byte >= 'a' && byte <= 'f') || (byte >= 'A' && byte <= 'F')) {
            ++cursor.offset;
            if (result.base != 16 && (byte == 'e' || byte == 'E' || byte == 'f' || byte == 'F')) return requiresReal();
            digit = byte >= 'a' ? byte - 'a' + 10 : byte - 'A' + 10;
        } else if (byte == 'L') {
            if (byteAt(cursor.offset + 1) == 'i') return requiresReal();
            break;
        } else if (byte == '.') {
            const auto next = byteAt(cursor.offset + 1);
            if (next == '.') break;
            if (result.base <= 10 && value > 0 &&
                    (detail::sourceNumberAlphabetic(next) || next == '_' || next >= 0x80)) break;
            const bool hex = (next >= '0' && next <= '9') || (next >= 'a' && next <= 'f') ||
                (next >= 'A' && next <= 'F');
            if ((result.base == 16 && !hex) || result.base == 2) break;
            return requiresReal();
        } else if (byte == 'i' || byte == 'p' || byte == 'P') return requiresReal();
        else if (byte == '_') { ++cursor.offset; continue; }
        else break;
        anyRadixDigit = true;
        if (!result.invalidDigit && digit >= result.base) result.invalidDigit = byte;
        constexpr auto maximum = (std::numeric_limits<std::uint64_t>::max)();
        if (value > maximum / result.base) result.overflowed = true;
        const auto product = value * result.base;
        if (product > maximum - digit) result.overflowed = true;
        value = product + digit;
    }
    bool error = false;
    if (result.invalidDigit) { diagnose(SourceIntegerIssue::InvalidDigit); error = true; }
    if (result.overflowed && !error) { diagnose(SourceIntegerIssue::Overflow); error = true; }
    if ((result.base == 2 || result.base == 16) && !anyRadixDigit) diagnose(SourceIntegerIssue::MissingRadixDigits);
    result.value = value;
    result.flags = result.base == 10 ? 1 : 0;
    result.suffixStart = cursor.offset;
    while (true) {
        const auto byte = byteAt(cursor.offset);
        unsigned flag = 0;
        if (byte == 'u' || byte == 'U') flag = 2;
        else if (byte == 'l' || byte == 'L') {
            flag = 4;
            if (byte == 'l') diagnose(SourceIntegerIssue::LowercaseLongSuffix);
        } else break;
        ++cursor.offset;
        if ((result.flags & flag) && !error) {
            diagnose(SourceIntegerIssue::RepeatedSuffix);
            result.repeatedSuffix = byte;
            result.repeatedSuffixOffset = cursor.offset - 1;
            error = true;
        }
        result.flags |= flag;
    }
    result.suffixLength = cursor.offset - result.suffixStart;
    if (result.base == 8 && value >= 8) diagnose(SourceIntegerIssue::UnsupportedOctal);
    const bool high64 = (value & 0x8000000000000000ull) != 0;
    switch (result.flags) {
    case 0:
        result.kind = high64 ? SourceIntegerKind::UInt64 : (value & 0xffffffff00000000ull) ?
            SourceIntegerKind::Int64 : (value & 0x80000000ull) ? SourceIntegerKind::UInt32 : SourceIntegerKind::Int32;
        break;
    case 1:
        result.kind = high64 ? SourceIntegerKind::UInt64 : (value & 0xffffffff80000000ull) ?
            SourceIntegerKind::Int64 : SourceIntegerKind::Int32;
        break;
    case 2: case 3:
        result.kind = value & 0xffffffff00000000ull ? SourceIntegerKind::UInt64 : SourceIntegerKind::UInt32;
        break;
    case 5:
        if (high64 && !error) diagnose(SourceIntegerIssue::SignedOverflow);
        [[fallthrough]];
    case 4: result.kind = high64 ? SourceIntegerKind::UInt64 : SourceIntegerKind::Int64; break;
    case 6: case 7: result.kind = SourceIntegerKind::UInt64; break;
    }
    result.length = cursor.offset - result.start;
    return result;
}
} // namespace nijigenerate::compat
