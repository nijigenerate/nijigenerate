#pragma once

// Adapted from LDC 1.41 dmd/lexer.d charConstant, D mode.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_escape.hpp"
#include "source_lexical_trivia.hpp"

namespace nijigenerate::compat {

enum class SourceCharacterKind { NotCharacter, Char, Wchar, Dchar };
enum class SourceCharacterIssue { None, Unterminated, MultipleCharacters, EntityRequiresString };
struct SourceCharacterLiteral {
    SourceCharacterKind kind = SourceCharacterKind::NotCharacter;
    char32_t value = 0;
    std::size_t start = 0;
    std::size_t length = 0;
    SourceLexicalTrivia lexical;
    SourceLexicalEscape escape;
    SourceCharacterIssue issue = SourceCharacterIssue::None;
    SourceCharacterIssue additionalIssue = SourceCharacterIssue::None;
};

constexpr SourceCharacterLiteral ngScanSourceCharacterLiteral(
        std::string_view source, SourceLexicalPosition position = {}) noexcept {
    SourceCharacterLiteral result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    auto& cursor = result.lexical.position;
    result.start = cursor.offset;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endOfInput ||
            source[cursor.offset] != '\'') return result;
    result.kind = SourceCharacterKind::Char;
    ++cursor.offset;
    const auto byteAt = [&](std::size_t at) -> unsigned char {
        return at < source.size() ? static_cast<unsigned char>(source[at]) : 0;
    };
    const auto diagnose = [&](SourceCharacterIssue issue) {
        if (result.issue == SourceCharacterIssue::None) result.issue = issue;
        else result.additionalIssue = issue;
    };
    const auto finish = [&] { result.length = cursor.offset - result.start; return result; };
    const auto retainScalarIssue = [&](const SourceLexicalScalar& scalar, std::size_t start) {
        if (scalar.issue != SourceLexicalIssue::None && result.lexical.issue == SourceLexicalIssue::None) {
            result.lexical.issue = scalar.issue;
            result.lexical.diagnosticOffset = start;
        }
    };
    const auto initial = byteAt(cursor.offset);
    if (initial == 0 || initial == 0x1a) {
        result.value = '?';
        diagnose(SourceCharacterIssue::Unterminated);
        return finish();
    }
    if (initial == '\n' || initial == '\r' || initial == '\'') {
        ++cursor.offset;
        if (initial == '\n') { ++cursor.line; cursor.lineStart = cursor.offset; }
        result.value = '?';
        diagnose(SourceCharacterIssue::Unterminated);
        return finish();
    }
    if (initial == '\\') {
        ++cursor.offset;
        const auto introducer = byteAt(cursor.offset);
        if (introducer == 'u') result.kind = SourceCharacterKind::Wchar;
        else if (introducer == 'U' || introducer == '&') result.kind = SourceCharacterKind::Dchar;
        result.escape = ngScanSourceLexicalEscape(source, cursor.offset);
        cursor.offset = result.escape.next;
        result.value = result.escape.value;
        if (result.escape.second != 0xffff) {
            diagnose(SourceCharacterIssue::EntityRequiresString);
            result.value = '?';
        }
    } else if (initial >= 0x80) {
        const auto start = cursor.offset;
        const auto scalar = ngDecodeSourceLexicalScalar(source, start);
        retainScalarIssue(scalar, start);
        cursor.offset = scalar.next;
        if (scalar.value == 0x2028 || scalar.value == 0x2029) {
            ++cursor.line;
            cursor.lineStart = cursor.offset;
            result.value = '?';
            diagnose(SourceCharacterIssue::Unterminated);
            return finish();
        }
        result.kind = scalar.value < 0xd800 || (scalar.value >= 0xe000 && scalar.value < 0xfffe) ?
            SourceCharacterKind::Wchar : SourceCharacterKind::Dchar;
        result.value = scalar.value;
    } else {
        result.value = initial;
        ++cursor.offset;
    }
    if (byteAt(cursor.offset) == '\'') {
        ++cursor.offset;
        return finish();
    }
    while (true) {
        const auto byte = byteAt(cursor.offset);
        if (byte == '\'' || byte == 0 || byte == 0x1a || byte == '\n' || byte == '\r' ||
                byte == ';' || byte == ')' || byte == ']' || byte == '}') break;
        if (byte >= 0x80) {
            const auto start = cursor.offset;
            const auto scalar = ngDecodeSourceLexicalScalar(source, start);
            retainScalarIssue(scalar, start);
            if (scalar.value == 0x2028 || scalar.value == 0x2029) break;
            cursor.offset = scalar.next;
        } else ++cursor.offset;
    }
    if (byteAt(cursor.offset) == '\'') {
        ++cursor.offset;
        diagnose(SourceCharacterIssue::MultipleCharacters);
    } else diagnose(SourceCharacterIssue::Unterminated);
    result.value = '?';
    return finish();
}
} // namespace nijigenerate::compat
