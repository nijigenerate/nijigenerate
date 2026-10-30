#pragma once

// Adapted from LDC 1.41 dmd/lexer.d interpolated string paths and
// handleInterpolatedSegment, and tokens.d appendInterpolatedPart.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_token_string.hpp"
#include <bit>

namespace nijigenerate::compat {

enum class SourceInterpolatedStringKind { NotInterpolated, Quoted, Raw, Token };
enum class SourceInterpolatedStringIssue { None, Unterminated, PendingProducer, InvalidToken, OutputCapacity };
struct SourceInterpolatedPart {
    std::size_t offset = 0;
    std::size_t length = 0;
    bool expression = false;
};
template<std::size_t Capacity, std::size_t PartCapacity> struct SourceInterpolatedStringLiteral {
    SourceInterpolatedStringKind kind = SourceInterpolatedStringKind::NotInterpolated;
    SourceLiteralBytes<Capacity> bytes;
    std::array<SourceInterpolatedPart, PartCapacity> parts{};
    std::size_t partCount = 0;
    SourceLexicalTrivia lexical;
    SourceLexicalEscape firstEscapeIssue;
    SourceInterpolatedStringIssue issue = SourceInterpolatedStringIssue::None;
    std::size_t start = 0;
    std::size_t length = 0;
    char postfix = 0;
};

// Both capacities are native carrier preconditions, not Source language limits.
// Parts own their bytes, including empty parts, and keep offsets rather than
// self-referential views. Scan supplies actual nested tokens and pending domains.
template<std::size_t Capacity, std::size_t PartCapacity, class Scan>
constexpr SourceInterpolatedStringLiteral<Capacity, PartCapacity> ngScanSourceInterpolatedStringLiteral(
        std::string_view source, SourceLexicalPosition position, Scan&& scan) {
    SourceInterpolatedStringLiteral<Capacity, PartCapacity> result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    auto& cursor = result.lexical.position;
    result.start = cursor.offset;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endOfInput) return result;
    const auto rest = source.substr(cursor.offset);
    if (rest.starts_with("i\"")) result.kind = SourceInterpolatedStringKind::Quoted;
    else if (rest.starts_with("i`")) result.kind = SourceInterpolatedStringKind::Raw;
    else if (rest.starts_with("iq{")) result.kind = SourceInterpolatedStringKind::Token;
    else return result;
    cursor.offset += result.kind == SourceInterpolatedStringKind::Token ? 3 : 2;
    std::size_t partStart = 0;
    std::size_t rawStart = cursor.offset;
    std::uint32_t braces = 1;
    const auto finish = [&] { result.length = cursor.offset - result.start; return result; };
    const auto byteAt = [&](std::size_t at) -> unsigned char {
        return at < source.size() ? static_cast<unsigned char>(source[at]) : 0;
    };
    const auto emit = [&](std::uint32_t byte) {
        if (result.bytes.length == Capacity) { result.issue = SourceInterpolatedStringIssue::OutputCapacity; return false; }
        result.bytes.data[result.bytes.length++] = static_cast<char>(byte & 0xff);
        return true;
    };
    const auto emitScalar = [&](char32_t scalar) {
        if (scalar <= 0x7f) return emit(scalar);
        if (scalar <= 0x7ff) return emit(0xc0 | (scalar >> 6)) && emit(0x80 | (scalar & 0x3f));
        if (scalar <= 0xffff)
            return emit(0xe0 | (scalar >> 12)) && emit(0x80 | ((scalar >> 6) & 0x3f)) && emit(0x80 | (scalar & 0x3f));
        return emit(0xf0 | (scalar >> 18)) && emit(0x80 | ((scalar >> 12) & 0x3f)) &&
            emit(0x80 | ((scalar >> 6) & 0x3f)) && emit(0x80 | (scalar & 0x3f));
    };
    const auto appendPart = [&](bool expression) {
        if (result.partCount == PartCapacity) { result.issue = SourceInterpolatedStringIssue::OutputCapacity; return false; }
        result.parts[result.partCount++] = {partStart, result.bytes.length - partStart, expression};
        partStart = result.bytes.length;
        return true;
    };
    const auto appendRaw = [&](std::size_t first, std::size_t last) {
        for (auto at = first; at < last; ++at) if (!emit(static_cast<unsigned char>(source[at]))) return false;
        return true;
    };
    const auto accept = [&](const SourceTokenStringStep& step) {
        result.lexical = step.lexical;
        switch (step.kind) {
        case SourceTokenStringStepKind::EndOfInput: result.issue = SourceInterpolatedStringIssue::Unterminated; return false;
        case SourceTokenStringStepKind::Pending: result.issue = SourceInterpolatedStringIssue::PendingProducer; return false;
        case SourceTokenStringStepKind::Invalid: result.issue = SourceInterpolatedStringIssue::InvalidToken; return false;
        case SourceTokenStringStepKind::OutputCapacity: result.issue = SourceInterpolatedStringIssue::OutputCapacity; return false;
        default: return true;
        }
    };
    // Called after an actual '$' with p pointing to the following byte.
    const auto interpolate = [&] {
        if (!appendPart(false)) return false;
        ++cursor.offset;
        const auto expressionStart = cursor.offset;
        std::int32_t parentheses = 1;
        while (parentheses > 0) {
            const auto previous = cursor.offset;
            const auto step = std::invoke(scan, source, cursor, result.kind == SourceInterpolatedStringKind::Token);
            if (!accept(step)) return false;
            if (step.kind == SourceTokenStringStepKind::LeftParenthesis)
                parentheses = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(parentheses) + 1u);
            else if (step.kind == SourceTokenStringStepKind::RightParenthesis) --parentheses;
            if (cursor.offset <= previous) { result.issue = SourceInterpolatedStringIssue::InvalidToken; return false; }
        }
        if (!appendRaw(expressionStart, cursor.offset - 1) || !appendPart(true)) return false;
        rawStart = cursor.offset;
        return true;
    };
    const auto postfix = [&] {
        const auto byte = byteAt(cursor.offset);
        if (byte == 'c' || byte == 'w' || byte == 'd') { result.postfix = static_cast<char>(byte); ++cursor.offset; }
    };
    while (true) {
        if (result.kind == SourceInterpolatedStringKind::Token) {
            const auto previous = cursor.offset;
            const auto step = std::invoke(scan, source, cursor, true);
            if (!accept(step)) return finish();
            if (step.kind == SourceTokenStringStepKind::LeftCurly) ++braces;
            else if (step.kind == SourceTokenStringStepKind::RightCurly && --braces == 0) {
                if (!appendRaw(rawStart, cursor.offset - 1) || !appendPart(false)) return finish();
                postfix();
                return finish();
            } else if (step.kind == SourceTokenStringStepKind::Dollar && byteAt(cursor.offset) == '(') {
                if (!appendRaw(rawStart, cursor.offset - 1) || !interpolate()) return finish();
            }
            if (cursor.offset <= previous) { result.issue = SourceInterpolatedStringIssue::InvalidToken; return finish(); }
            continue;
        }
        const auto first = cursor.offset;
        const auto byte = byteAt(first);
        if (byte == 0 || byte == 0x1a) { result.issue = SourceInterpolatedStringIssue::Unterminated; return finish(); }
        ++cursor.offset;
        const char terminator = result.kind == SourceInterpolatedStringKind::Quoted ? '"' : '`';
        if (byte == terminator) {
            if (!appendPart(false)) return finish();
            postfix();
            return finish();
        }
        if (byte == '$' && byteAt(cursor.offset) == '(') {
            if (!interpolate()) return finish();
            continue;
        }
        if (byte == '\\' && result.kind == SourceInterpolatedStringKind::Quoted) {
            const auto introducer = byteAt(cursor.offset);
            if (introducer == '$') { ++cursor.offset; if (!emit('$')) return finish(); continue; }
            const auto escape = ngScanSourceLexicalEscape(source, cursor.offset);
            cursor.offset = escape.next;
            if (escape.issue != SourceEscapeIssue::None && result.firstEscapeIssue.issue == SourceEscapeIssue::None)
                result.firstEscapeIssue = escape;
            if (introducer == 'u' || introducer == 'U' || introducer == '&') {
                if (!emitScalar(escape.value)) return finish();
                if (escape.second != 0xffff && !emitScalar(escape.second)) return finish();
            } else if (!emit(escape.value)) return finish();
            continue;
        }
        if (byte == '\r' || byte == '\n') {
            if (byte == '\r' && byteAt(cursor.offset) == '\n') ++cursor.offset;
            ++cursor.line;
            cursor.lineStart = cursor.offset;
            if (!emit('\n')) return finish();
            continue;
        }
        if (byte >= 0x80) {
            const auto scalar = ngDecodeSourceLexicalScalar(source, first);
            cursor.offset = scalar.next;
            if (scalar.issue != SourceLexicalIssue::None) {
                result.lexical.issue = scalar.issue;
                result.lexical.diagnosticOffset = first;
                return finish();
            }
            auto value = scalar.value;
            if (value == 0x2028 || value == 0x2029) {
                ++cursor.line;
                cursor.lineStart = cursor.offset;
                if (result.kind == SourceInterpolatedStringKind::Quoted) {
                    cursor.lineStart = cursor.offset - 1;
                    value = '\n';
                }
            }
            if (!emitScalar(value)) return finish();
        } else if (!emit(byte)) return finish();
    }
}

} // namespace nijigenerate::compat
