#pragma once

// Adapted from LDC 1.41 dmd/lexer.d and dmd/root/utf.d.
// Copyright (C) 1999-2025 The D Language Foundation.
// Boost Software License 1.0; see native/port/licenses/source-expression-lexical.txt.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nijigenerate::compat {

enum class SourceLexicalIssue {
    None, InvalidPosition, OutsideCodeSpace, TruncatedSequence, OverlongSequence,
    InvalidTrailer, InvalidCodePoint, BidiControl, UnterminatedBlockComment, UnterminatedNestedComment
};

constexpr std::string_view ngSourceLexicalIssueMessage(SourceLexicalIssue issue) noexcept {
    switch (issue) {
    case SourceLexicalIssue::None: return {};
    case SourceLexicalIssue::InvalidPosition: return "Source lexical cursor is outside its input";
    case SourceLexicalIssue::OutsideCodeSpace: return "Outside Unicode code space";
    case SourceLexicalIssue::TruncatedSequence: return "Truncated UTF-8 sequence";
    case SourceLexicalIssue::OverlongSequence: return "Overlong UTF-8 sequence";
    case SourceLexicalIssue::InvalidTrailer: return "Invalid trailing code unit";
    case SourceLexicalIssue::InvalidCodePoint: return "Invalid code point decoded";
    case SourceLexicalIssue::BidiControl:
        return "Bidirectional control characters are disallowed for security reasons.";
    case SourceLexicalIssue::UnterminatedBlockComment: return "unterminated /* */ comment";
    case SourceLexicalIssue::UnterminatedNestedComment: return "unterminated /+ +/ comment";
    }
    return {};
}

struct SourceLexicalScalar {
    char32_t value = 0;
    std::size_t next = 0;
    SourceLexicalIssue issue = SourceLexicalIssue::None;
};

constexpr bool ngSourceLexicalBidiControl(char32_t value) noexcept {
    return value == 0x061c || value == 0x200e || value == 0x200f ||
        (value >= 0x202a && value <= 0x202e) || (value >= 0x2066 && value <= 0x2069);
}

// The selected compiler's decoder advances one byte on an error, even when
// validation examined more bytes. Its lexer bounds the input at a NUL sentinel.
constexpr SourceLexicalScalar ngDecodeSourceLexicalScalar(
        std::string_view source, std::size_t offset) noexcept {
    if (offset >= source.size()) return {0, offset, SourceLexicalIssue::InvalidPosition};
    const auto first = static_cast<unsigned char>(source[offset]);
    SourceLexicalScalar result{first, offset + 1};
    if (first < 0x80) return result;
    const std::size_t width = first >= 0xc0 && first <= 0xdf ? 2 :
        first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf7 ? 4 : 0;
    if (!width) { result.issue = SourceLexicalIssue::OutsideCodeSpace; return result; }
    std::size_t available = 1;
    while (available < 4 && available < source.size() - offset && source[offset + available] != '\0')
        ++available;
    if (available < width) { result.issue = SourceLexicalIssue::TruncatedSequence; return result; }
    const auto second = static_cast<unsigned char>(source[offset + 1]);
    if ((first & 0xfe) == 0xc0 || (first == 0xe0 && (second & 0xe0) == 0x80) ||
            (first == 0xf0 && (second & 0xf0) == 0x80)) {
        result.issue = SourceLexicalIssue::OverlongSequence;
        return result;
    }
    char32_t value = first & ((1u << (7 - width)) - 1u);
    for (std::size_t i = 1; i < width; ++i) {
        const auto byte = static_cast<unsigned char>(source[offset + i]);
        if ((byte & 0xc0) != 0x80) { result.issue = SourceLexicalIssue::InvalidTrailer; return result; }
        value = (value << 6) | (byte & 0x3f);
    }
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        result.issue = SourceLexicalIssue::InvalidCodePoint;
        return result;
    }
    result.value = value;
    result.next = offset + width;
    if (ngSourceLexicalBidiControl(value)) result.issue = SourceLexicalIssue::BidiControl;
    return result;
}

struct SourceLexicalPosition {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t lineStart = 0;
};
struct SourceLexicalTrivia {
    SourceLexicalPosition position;
    SourceLexicalIssue issue = SourceLexicalIssue::None;
    std::size_t diagnosticOffset = 0;
    bool consumed = false;
    bool endOfInput = false;
    bool endedInLineComment = false;
    bool endOfLine = false;
};

constexpr bool ngSourceLexicalAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\v' || value == '\f' || value == '\r' || value == '\n';
}

// This is the whitespace/comment stage of the D expression lexer, not the
// runtime selector lexer or a D expression parser. It reports the first lexical
// failure; a caller must not accept tokens following a reported failure.
constexpr SourceLexicalTrivia ngScanSourceLexicalTrivia(
        std::string_view source, SourceLexicalPosition position = {}, bool tokenizeNewlines = false) noexcept {
    SourceLexicalTrivia result{position};
    auto& cursor = result.position;
    if (cursor.offset > source.size()) {
        result.issue = SourceLexicalIssue::InvalidPosition;
        result.diagnosticOffset = cursor.offset;
        return result;
    }
    const auto atEnd = [&] {
        return cursor.offset == source.size() || source[cursor.offset] == '\0' ||
            static_cast<unsigned char>(source[cursor.offset]) == 0x1a;
    };
    const auto pair = [&](char first, char second) {
        return source.size() - cursor.offset >= 2 && source[cursor.offset] == first &&
            source[cursor.offset + 1] == second;
    };
    const auto advance = [&](bool blockComment, bool lineComment) {
        const auto start = cursor.offset;
        const char byte = source[start];
        if (byte == '\r' || byte == '\n') {
            ++cursor.offset;
            const bool hasLineFeed = byte == '\n' ||
                (byte == '\r' && cursor.offset < source.size() && source[cursor.offset] == '\n');
            if (byte == '\r' && hasLineFeed)
                ++cursor.offset;
            ++cursor.line;
            // Block comments call endOfLine before advancing past LF.
            cursor.lineStart = blockComment && hasLineFeed ? cursor.offset - 1 : cursor.offset;
        } else if (static_cast<unsigned char>(byte) < 0x80) ++cursor.offset;
        else {
            const auto scalar = ngDecodeSourceLexicalScalar(source, start);
            cursor.offset = scalar.next;
            if (scalar.issue != SourceLexicalIssue::None) {
                result.issue = scalar.issue;
                result.diagnosticOffset = start;
                return;
            }
            if (scalar.value == 0x2028 || scalar.value == 0x2029) {
                ++cursor.line;
                // The selected scanner advances past the final UTF byte before
                // endOfLine only in the line-comment branch.
                cursor.lineStart = lineComment ? cursor.offset : cursor.offset - 1;
            }
        }
    };
    while (!atEnd()) {
        if (ngSourceLexicalAsciiWhitespace(source[cursor.offset])) {
            const bool newline = source[cursor.offset] == '\r' || source[cursor.offset] == '\n';
            result.consumed = true;
            advance(false, false);
            if (tokenizeNewlines && newline) { result.endOfLine = true; return result; }
            continue;
        }
        if (pair('/', '/')) {
            result.consumed = true;
            cursor.offset += 2;
            while (!atEnd()) {
                const auto oldLine = cursor.line;
                advance(false, true);
                if (result.issue != SourceLexicalIssue::None) return result;
                if (cursor.line != oldLine) break;
            }
            if (atEnd()) {
                result.endOfInput = true;
                // A trailing newline closes the comment before this sentinel.
                result.endedInLineComment = cursor.lineStart != cursor.offset;
                return result;
            }
            continue;
        }
        if (pair('/', '*') || pair('/', '+')) {
            result.consumed = true;
            const auto opening = cursor.offset;
            const bool nested = source[cursor.offset + 1] == '+';
            cursor.offset += 2;
            std::size_t depth = 1;
            while (depth) {
                if (atEnd()) {
                    result.endOfInput = true;
                    result.issue = nested ? SourceLexicalIssue::UnterminatedNestedComment :
                        SourceLexicalIssue::UnterminatedBlockComment;
                    result.diagnosticOffset = opening;
                    return result;
                }
                if (nested && pair('/', '+')) { ++depth; cursor.offset += 2; }
                else if (pair(nested ? '+' : '*', '/')) { --depth; cursor.offset += 2; }
                else {
                    advance(true, false);
                    if (result.issue != SourceLexicalIssue::None) return result;
                }
            }
            continue;
        }
        if (static_cast<unsigned char>(source[cursor.offset]) >= 0x80) {
            const auto scalar = ngDecodeSourceLexicalScalar(source, cursor.offset);
            if (scalar.issue != SourceLexicalIssue::None) {
                result.diagnosticOffset = cursor.offset;
                result.issue = scalar.issue;
                cursor.offset = scalar.next;
                return result;
            }
            if (scalar.value == 0x2028 || scalar.value == 0x2029) {
                result.consumed = true;
                advance(false, false);
                if (tokenizeNewlines) { result.endOfLine = true; return result; }
                continue;
            }
        }
        return result;
    }
    result.endOfInput = true;
    return result;
}
} // namespace nijigenerate::compat
