#pragma once

// Adapted from LDC 1.41 dmd/lexer.d escapeSequence, D mode.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_named_entities.hpp"
#include <cstddef>
#include <cstdint>

namespace nijigenerate::compat {

enum class SourceEscapeKind { Simple, ByteHex, Unicode16, Unicode32, Octal, Entity };
enum class SourceEscapeIssue {
    None, InvalidPosition, UndefinedSequence, UndefinedHexSequence, WrongHexDigitCount,
    InvalidUnicodeScalar, UnknownEntity, UnterminatedEntity, OctalOverflow
};
struct SourceLexicalEscape {
    char32_t value = 0;
    char32_t second = 0xffff; // Source dchar.init, not a second emitted character.
    std::size_t next = 0;
    SourceEscapeKind kind = SourceEscapeKind::Simple;
    SourceEscapeIssue issue = SourceEscapeIssue::None;
    SourceEscapeIssue additionalIssue = SourceEscapeIssue::None;
    std::size_t actualDigits = 0;
    std::size_t expectedDigits = 0;
    std::size_t diagnosticOffset = 0;
    std::size_t diagnosticLength = 0;
    std::uint32_t diagnosticValue = 0;
    bool utf16Surrogate = false;
};

namespace detail {
constexpr int sourceEscapeHexDigit(unsigned char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}
constexpr bool sourceEscapeAlphabetic(unsigned char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}
}

// offset points AFTER the backslash. The input has a virtual NUL at its bounded
// end. Recovery never reads or advances past that boundary. Diagnostic values
// preserve the selected decoder's data; formatting/emission belongs to its
// compiler diagnostic sink, not application runtime exceptions.
constexpr SourceLexicalEscape ngScanSourceLexicalEscape(std::string_view source, std::size_t offset) noexcept {
    SourceLexicalEscape result;
    result.next = offset;
    result.diagnosticOffset = offset;
    if (offset > source.size()) { result.issue = SourceEscapeIssue::InvalidPosition; return result; }
    const auto byteAt = [&](std::size_t at) -> unsigned char {
        return at < source.size() ? static_cast<unsigned char>(source[at]) : 0;
    };
    const auto advance = [&] { if (result.next < source.size()) ++result.next; };
    const auto diagnose = [&](SourceEscapeIssue issue) {
        if (result.issue == SourceEscapeIssue::None) result.issue = issue;
        else result.additionalIssue = issue;
    };
    const auto initial = byteAt(offset);
    result.value = initial;
    switch (initial) {
    case '\'': case '"': case '?': case '\\': advance(); return result;
    case 'a': result.value = 7; advance(); return result;
    case 'b': result.value = 8; advance(); return result;
    case 'f': result.value = 12; advance(); return result;
    case 'n': result.value = 10; advance(); return result;
    case 'r': result.value = 13; advance(); return result;
    case 't': result.value = 9; advance(); return result;
    case 'v': result.value = 11; advance(); return result;
    case 'x': case 'u': case 'U': {
        result.kind = initial == 'x' ? SourceEscapeKind::ByteHex :
            initial == 'u' ? SourceEscapeKind::Unicode16 : SourceEscapeKind::Unicode32;
        result.expectedDigits = initial == 'x' ? 2 : initial == 'u' ? 4 : 8;
        advance();
        auto digit = detail::sourceEscapeHexDigit(byteAt(result.next));
        if (digit < 0) {
            result.value = byteAt(result.next);
            result.diagnosticValue = result.value;
            diagnose(SourceEscapeIssue::UndefinedHexSequence);
            advance();
            return result;
        }
        std::uint32_t value = 0;
        while (true) {
            value = value * 16 + static_cast<unsigned>(digit);
            advance();
            if (++result.actualDigits == result.expectedDigits) break;
            digit = detail::sourceEscapeHexDigit(byteAt(result.next));
            if (digit < 0) { diagnose(SourceEscapeIssue::WrongHexDigitCount); break; }
        }
        result.diagnosticValue = value;
        if (initial != 'x' && (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))) {
            diagnose(SourceEscapeIssue::InvalidUnicodeScalar);
            result.utf16Surrogate = value >= 0xd800 && value <= 0xdfff;
            value = '?';
        }
        result.value = value;
        return result;
    }
    case '&': {
        result.kind = SourceEscapeKind::Entity;
        advance();
        const auto start = result.next;
        while (true) {
            const auto byte = byteAt(result.next);
            if (byte == ';') {
                const auto entity = ngSourceNamedEntity(source.substr(start, result.next - start));
                result.diagnosticOffset = start;
                result.diagnosticLength = result.next - start;
                result.value = entity.first;
                if (!entity.first && !entity.second) {
                    diagnose(SourceEscapeIssue::UnknownEntity);
                    result.value = '?';
                }
                if (entity.second) result.second = entity.second;
                advance();
                return result;
            }
            if (detail::sourceEscapeAlphabetic(byte) ||
                    (result.next != start && byte >= '0' && byte <= '9')) {
                advance();
                continue;
            }
            diagnose(SourceEscapeIssue::UnterminatedEntity);
            result.diagnosticOffset = start;
            result.diagnosticLength = result.next - start + (result.next < source.size() ? 1 : 0);
            result.diagnosticValue = byte;
            result.value = '?';
            return result;
        }
    }
    case 0: case 0x1a:
        result.value = '\\';
        return result;
    default:
        if (initial >= '0' && initial <= '7') {
            result.kind = SourceEscapeKind::Octal;
            std::uint32_t value = 0;
            do {
                value = value * 8 + byteAt(result.next) - '0';
                advance();
                ++result.actualDigits;
            } while (result.actualDigits < 3 && byteAt(result.next) >= '0' && byteAt(result.next) <= '7');
            result.value = value;
            result.diagnosticValue = value;
            if (value > 0xff) diagnose(SourceEscapeIssue::OctalOverflow);
        } else {
            result.diagnosticValue = initial;
            diagnose(SourceEscapeIssue::UndefinedSequence);
            advance();
        }
        return result;
    }
}
} // namespace nijigenerate::compat
