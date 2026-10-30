#pragma once

// Adapted from LDC 1.41 dmd/lexer.d delimitedStringConstant.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_identifiers.hpp"
#include "source_lexical_plain_string.hpp"

namespace nijigenerate::compat {

enum class SourceDelimitedStringIssue {
    None, Unterminated, IdentifierExpected, WhitespaceDelimiter, NonblankHeredocLine,
    MissingClosingQuote, OutputCapacity, PendingDelimiterToken
};

template<std::size_t Capacity> struct SourceDelimitedStringLiteral {
    bool recognized = false;
    SourceLiteralBytes<Capacity> bytes;
    SourceLexicalTrivia lexical;
    SourceDelimitedStringIssue issue = SourceDelimitedStringIssue::None;
    SourceDelimitedStringIssue additionalIssue = SourceDelimitedStringIssue::None;
    std::string_view heredoc;
    char32_t left = 0;
    char32_t right = 0;
    std::uint32_t nesting = 1;
    std::size_t start = 0;
    std::size_t length = 0;
    std::size_t diagnosticOffset = 0;
    char postfix = 0;
};

// Capacity bounds only the native output carrier. Token/interpolated strings
// have different producers. Unsupported malformed delimiter tokens are explicit,
// rather than supplying an invented successful identifier or compiler recovery.
template<std::size_t Capacity>
constexpr SourceDelimitedStringLiteral<Capacity> ngScanSourceDelimitedStringLiteral(
        std::string_view source, SourceLexicalPosition position = {},
        SourceIdentifierTable identifiers = SourceIdentifierTable::LeastRestrictive) noexcept {
    SourceDelimitedStringLiteral<Capacity> result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    auto& cursor = result.lexical.position;
    result.start = cursor.offset;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endOfInput ||
            !source.substr(cursor.offset).starts_with("q\"")) return result;
    result.recognized = true;
    cursor.offset += 2;
    bool paired = true;
    bool blankRestOfLine = false;
    bool startOfLine = false;
    const auto byteAt = [&](std::size_t at) -> unsigned char {
        return at < source.size() ? static_cast<unsigned char>(source[at]) : 0;
    };
    const auto diagnose = [&](SourceDelimitedStringIssue issue, std::size_t offset) {
        if (result.issue == SourceDelimitedStringIssue::None) {
            result.issue = issue;
            result.diagnosticOffset = offset;
        } else if (result.additionalIssue == SourceDelimitedStringIssue::None) result.additionalIssue = issue;
    };
    const auto emit = [&](std::size_t first, std::size_t last, char32_t scalar) {
        const bool normalizedReturn = source[first] == '\r';
        const std::size_t count = normalizedReturn ? 1 : last - first;
        if (count > Capacity - result.bytes.length) {
            diagnose(SourceDelimitedStringIssue::OutputCapacity, first);
            return false;
        }
        if (normalizedReturn) result.bytes.data[result.bytes.length++] = static_cast<char>(scalar);
        else for (auto at = first; at < last; ++at) result.bytes.data[result.bytes.length++] = source[at];
        return true;
    };
    const auto finish = [&] { result.length = cursor.offset - result.start; return result; };
    while (true) {
        const auto first = cursor.offset;
        const auto byte = byteAt(first);
        if (byte == 0 || byte == 0x1a) {
            diagnose(SourceDelimitedStringIssue::Unterminated, first);
            return finish();
        }
        char32_t scalar = byte;
        ++cursor.offset;
        if (byte >= 0x80) {
            const auto decoded = ngDecodeSourceLexicalScalar(source, first);
            cursor.offset = decoded.next;
            if (decoded.issue != SourceLexicalIssue::None) {
                result.lexical.issue = decoded.issue;
                result.lexical.diagnosticOffset = first;
                return finish();
            }
            scalar = decoded.value;
        }
        if (scalar == '\r' && byteAt(cursor.offset) == '\n') continue;
        if (scalar == '\r') scalar = '\n';
        if (scalar == '\n' || scalar == 0x2028 || scalar == 0x2029) {
            ++cursor.line;
            cursor.lineStart = cursor.offset;
            startOfLine = true;
            if (blankRestOfLine) { blankRestOfLine = false; continue; }
            if (!result.heredoc.empty()) {
                if (!emit(first, cursor.offset, scalar)) return finish();
                continue;
            }
        }
        if (result.left == 0) {
            result.left = scalar;
            result.nesting = 1;
            if (scalar == '(') result.right = ')';
            else if (scalar == '{') result.right = '}';
            else if (scalar == '[') result.right = ']';
            else if (scalar == '<') result.right = '>';
            else if (ngSourceIdentifierStart(scalar, identifiers)) {
                auto saved = cursor;
                saved.offset = first;
                const auto token = ngScanSourceLexicalIdentifier(source, saved, identifiers);
                cursor = token.lexical.position;
                if (token.lexical.issue != SourceLexicalIssue::None || token.invalidContinuation) {
                    result.lexical = token.lexical;
                    diagnose(SourceDelimitedStringIssue::IdentifierExpected, first);
                    return finish();
                }
                if (token.kind == SourceIdentifierKind::LiteralPrefix) {
                    diagnose(SourceDelimitedStringIssue::PendingDelimiterToken, first);
                    return finish();
                }
                if (token.kind != SourceIdentifierKind::Identifier) {
                    diagnose(SourceDelimitedStringIssue::IdentifierExpected, first);
                    result.right = scalar;
                } else {
                    result.heredoc = source.substr(token.start, token.length);
                    blankRestOfLine = true;
                }
                paired = false;
            } else {
                result.right = scalar;
                paired = false;
                if (scalar < 0x80 && ngSourceLexicalAsciiWhitespace(static_cast<char>(scalar)))
                    diagnose(SourceDelimitedStringIssue::WhitespaceDelimiter, first);
            }
            continue;
        }
        if (blankRestOfLine) {
            diagnose(SourceDelimitedStringIssue::NonblankHeredocLine, first);
            blankRestOfLine = false;
            continue;
        }
        bool closed = false;
        if (paired) {
            if (scalar == result.left) ++result.nesting;
            else if (scalar == result.right) closed = --result.nesting == 0;
        } else if (scalar == result.right) closed = true;
        if (!closed && startOfLine && !result.heredoc.empty() && ngSourceIdentifierStart(scalar, identifiers)) {
            auto saved = cursor;
            saved.offset = first;
            const auto token = ngScanSourceLexicalIdentifier(source, saved, identifiers);
            if (token.kind == SourceIdentifierKind::Identifier && !token.invalidContinuation &&
                    token.lexical.issue == SourceLexicalIssue::None &&
                    source.substr(token.start, token.length) == result.heredoc) {
                cursor = token.lexical.position;
                closed = true;
            }
        }
        if (closed) {
            if (byteAt(cursor.offset) == '"') ++cursor.offset;
            else diagnose(SourceDelimitedStringIssue::MissingClosingQuote, cursor.offset);
            const auto postfix = byteAt(cursor.offset);
            if (postfix == 'c' || postfix == 'w' || postfix == 'd') {
                result.postfix = static_cast<char>(postfix);
                ++cursor.offset;
            }
            return finish();
        }
        if (!emit(first, cursor.offset, scalar)) return finish();
        startOfLine = false;
    }
}

} // namespace nijigenerate::compat
