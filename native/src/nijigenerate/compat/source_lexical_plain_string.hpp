#pragma once

// Adapted from LDC 1.41 dmd/lexer.d noninterpolated string paths.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_escape.hpp"
#include "source_lexical_trivia.hpp"
#include <array>

namespace nijigenerate::compat {

enum class SourcePlainStringKind { NotPlainString, Quoted, Raw, Hexadecimal };
enum class SourcePlainStringIssue { None, Unterminated, OddHexCount, NonHexCharacter, OutputCapacity, InvalidScalar };

template<std::size_t Capacity> struct SourceLiteralBytes {
    std::array<char, Capacity> data{};
    std::size_t length = 0;
    constexpr std::string_view view() const noexcept {
        return length ? std::string_view(data.data(), length) : std::string_view{};
    }
};

template<std::size_t Capacity> struct SourcePlainStringLiteral {
    SourcePlainStringKind kind = SourcePlainStringKind::NotPlainString;
    SourceLiteralBytes<Capacity> bytes;
    SourceLexicalTrivia lexical;
    SourceLexicalEscape firstEscapeIssue;
    SourcePlainStringIssue issue = SourcePlainStringIssue::None;
    SourcePlainStringIssue additionalIssue = SourcePlainStringIssue::None;
    std::size_t start = 0;
    std::size_t openingQuote = 0;
    std::size_t length = 0;
    std::size_t diagnosticOffset = 0;
    std::uint32_t hexCharacters = 0;
    char32_t diagnosticScalar = 0;
    char postfix = 0;
};

// Complete quoted, backtick/r-quoted and hex-string paths. q/token/interpolated
// literals belong to their separate stages and remain unconsumed. Capacity is
// a native caller precondition, not an invented Source maximum string length.
template<std::size_t Capacity>
constexpr SourcePlainStringLiteral<Capacity> ngScanSourcePlainStringLiteral(
        std::string_view source, SourceLexicalPosition position = {}) noexcept {
    SourcePlainStringLiteral<Capacity> result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    auto& cursor = result.lexical.position;
    result.start = cursor.offset;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endOfInput) return result;
    const auto rest = source.substr(cursor.offset);
    if (rest.front() == '"') result.kind = SourcePlainStringKind::Quoted;
    else if (rest.front() == '`') result.kind = SourcePlainStringKind::Raw;
    else if (rest.starts_with("r\"")) { result.kind = SourcePlainStringKind::Raw; ++cursor.offset; }
    else if (rest.starts_with("x\"")) { result.kind = SourcePlainStringKind::Hexadecimal; ++cursor.offset; }
    else return result;
    result.openingQuote = cursor.offset;
    const auto terminator = source[cursor.offset++];
    const auto byteAt = [&](std::size_t at) -> unsigned char {
        return at < source.size() ? static_cast<unsigned char>(source[at]) : 0;
    };
    const auto diagnose = [&](SourcePlainStringIssue issue, std::size_t offset) {
        if (result.issue == SourcePlainStringIssue::None) {
            result.issue = issue;
            result.diagnosticOffset = offset;
        } else if (result.additionalIssue == SourcePlainStringIssue::None) result.additionalIssue = issue;
    };
    const auto emitByte = [&](std::uint32_t value) {
        if (result.bytes.length == Capacity) {
            diagnose(SourcePlainStringIssue::OutputCapacity, cursor.offset);
            return false;
        }
        result.bytes.data[result.bytes.length++] = static_cast<char>(value & 0xff);
        return true;
    };
    const auto emitScalar = [&](char32_t value) {
        if (value <= 0x7f) return emitByte(value);
        if (value <= 0x7ff)
            return emitByte(0xc0 | (value >> 6)) && emitByte(0x80 | (value & 0x3f));
        if (value <= 0xffff && !(value >= 0xd800 && value <= 0xdfff))
            return emitByte(0xe0 | (value >> 12)) && emitByte(0x80 | ((value >> 6) & 0x3f)) &&
                emitByte(0x80 | (value & 0x3f));
        if (value >= 0x10000 && value <= 0x10ffff)
            return emitByte(0xf0 | (value >> 18)) && emitByte(0x80 | ((value >> 12) & 0x3f)) &&
                emitByte(0x80 | ((value >> 6) & 0x3f)) && emitByte(0x80 | (value & 0x3f));
        diagnose(SourcePlainStringIssue::InvalidScalar, cursor.offset);
        result.diagnosticScalar = value;
        return false;
    };
    const auto finish = [&] { result.length = cursor.offset - result.start; return result; };
    const auto postfix = [&] {
        const auto byte = byteAt(cursor.offset);
        if (byte == 'c' || byte == 'w' || byte == 'd') { result.postfix = static_cast<char>(byte); ++cursor.offset; }
    };
    std::uint32_t hexValue = ~std::uint32_t{0};
    while (true) {
        const auto characterStart = cursor.offset;
        auto byte = byteAt(cursor.offset);
        if (byte == 0 || byte == 0x1a) {
            diagnose(SourcePlainStringIssue::Unterminated, result.openingQuote);
            result.bytes.length = 0;
            return finish();
        }
        ++cursor.offset;
        if (byte == static_cast<unsigned char>(terminator)) {
            if (result.kind == SourcePlainStringKind::Hexadecimal && (result.hexCharacters & 1)) {
                diagnose(SourcePlainStringIssue::OddHexCount, characterStart);
                if (!emitByte(hexValue)) return finish();
            }
            postfix();
            return finish();
        }
        if (byte == '\r' || byte == '\n') {
            if (byte == '\r' && byteAt(cursor.offset) == '\n') ++cursor.offset;
            ++cursor.line;
            cursor.lineStart = cursor.offset;
            if (result.kind != SourcePlainStringKind::Hexadecimal && !emitByte('\n')) return finish();
            continue;
        }
        if (result.kind == SourcePlainStringKind::Hexadecimal &&
                (byte == ' ' || byte == '\t' || byte == '\v' || byte == '\f')) continue;
        if (result.kind == SourcePlainStringKind::Quoted && byte == '\\') {
            const auto introducer = byteAt(cursor.offset);
            const auto escape = ngScanSourceLexicalEscape(source, cursor.offset);
            cursor.offset = escape.next;
            if (escape.issue != SourceEscapeIssue::None && result.firstEscapeIssue.issue == SourceEscapeIssue::None)
                result.firstEscapeIssue = escape;
            if (introducer == 'u' || introducer == 'U' || introducer == '&') {
                if (!emitScalar(escape.value)) return finish();
                if (escape.second != 0xffff && !emitScalar(escape.second)) return finish();
            } else if (!emitByte(escape.value)) return finish();
            continue;
        }
        if (result.kind == SourcePlainStringKind::Hexadecimal) {
            std::uint32_t nibble = byte;
            const auto digit = detail::sourceEscapeHexDigit(byte);
            if (digit >= 0) nibble = static_cast<unsigned>(digit);
            else if (byte >= 0x80) {
                const auto scalar = ngDecodeSourceLexicalScalar(source, characterStart);
                cursor.offset = scalar.next;
                if (scalar.issue != SourceLexicalIssue::None && result.lexical.issue == SourceLexicalIssue::None) {
                    result.lexical.issue = scalar.issue;
                    result.lexical.diagnosticOffset = characterStart;
                }
                if (scalar.value == 0x2028 || scalar.value == 0x2029) {
                    ++cursor.line;
                    cursor.lineStart = cursor.offset;
                } else {
                    diagnose(SourcePlainStringIssue::NonHexCharacter, characterStart);
                    result.diagnosticScalar = scalar.value;
                }
                // The selected branch retains the first input byte as c;
                // decodeUTF's scalar is used for diagnostics/line state only.
            } else {
                diagnose(SourcePlainStringIssue::NonHexCharacter, characterStart);
                result.diagnosticScalar = byte;
            }
            if (result.hexCharacters & 1) {
                hexValue = (hexValue << 4) | nibble;
                if (!emitByte(hexValue)) return finish();
            } else hexValue = nibble;
            ++result.hexCharacters;
            continue;
        }
        if (byte >= 0x80) {
            const auto scalar = ngDecodeSourceLexicalScalar(source, characterStart);
            cursor.offset = scalar.next;
            if (scalar.issue != SourceLexicalIssue::None && result.lexical.issue == SourceLexicalIssue::None) {
                result.lexical.issue = scalar.issue;
                result.lexical.diagnosticOffset = characterStart;
            }
            auto value = scalar.value;
            if (value == 0x2028 || value == 0x2029) {
                ++cursor.line;
                if (result.kind == SourcePlainStringKind::Quoted) {
                    cursor.lineStart = cursor.offset - 1;
                    value = '\n';
                } else cursor.lineStart = cursor.offset;
            }
            if (!emitScalar(value)) return finish();
        } else if (!emitByte(byte)) return finish();
    }
}
} // namespace nijigenerate::compat
