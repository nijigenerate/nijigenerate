#pragma once

// Adapted from LDC 1.41 dmd/lexer.d tokenStringConstant, noninterpolated path.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_plain_string.hpp"
#include <functional>

namespace nijigenerate::compat {

enum class SourceTokenStringStepKind {
    Other, LeftCurly, RightCurly, LeftParenthesis, RightParenthesis, Dollar,
    EndOfInput, Pending, Invalid, OutputCapacity
};
struct SourceTokenStringStep {
    SourceTokenStringStepKind kind = SourceTokenStringStepKind::Invalid;
    SourceLexicalTrivia lexical;
};
enum class SourceTokenStringIssue { None, Unterminated, PendingProducer, InvalidToken, OutputCapacity };
template<std::size_t Capacity> struct SourceTokenStringLiteral {
    bool recognized = false;
    SourceLiteralBytes<Capacity> bytes;
    SourceLexicalTrivia lexical;
    SourceTokenStringIssue issue = SourceTokenStringIssue::None;
    std::uint32_t nesting = 1;
    std::size_t start = 0;
    std::size_t length = 0;
    char postfix = 0;
};

// Scan is the actual token producer, invoked in token-string mode. Braces in
// comments, nested strings and characters are resolved by that producer rather
// than guessed by counting source bytes. The returned string keeps raw bytes.
template<std::size_t Capacity, class Scan>
constexpr SourceTokenStringLiteral<Capacity> ngScanSourceTokenStringLiteral(
        std::string_view source, SourceLexicalPosition position, Scan&& scan) {
    SourceTokenStringLiteral<Capacity> result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    auto& cursor = result.lexical.position;
    result.start = cursor.offset;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endOfInput ||
            !source.substr(cursor.offset).starts_with("q{")) return result;
    result.recognized = true;
    cursor.offset += 2;
    const auto body = cursor.offset;
    const auto finish = [&] { result.length = cursor.offset - result.start; return result; };
    while (true) {
        const auto previous = cursor.offset;
        const auto step = std::invoke(scan, source, cursor);
        result.lexical = step.lexical;
        switch (step.kind) {
        case SourceTokenStringStepKind::LeftCurly:
            ++result.nesting;
            break;
        case SourceTokenStringStepKind::RightCurly:
            if (--result.nesting == 0) {
                const auto count = cursor.offset - 1 - body;
                if (count > Capacity) { result.issue = SourceTokenStringIssue::OutputCapacity; return finish(); }
                for (std::size_t i = 0; i < count; ++i) result.bytes.data[i] = source[body + i];
                result.bytes.length = count;
                if (cursor.offset < source.size()) {
                    const auto postfix = source[cursor.offset];
                    if (postfix == 'c' || postfix == 'w' || postfix == 'd') {
                        result.postfix = postfix;
                        ++cursor.offset;
                    }
                }
                return finish();
            }
            break;
        case SourceTokenStringStepKind::EndOfInput:
            result.issue = SourceTokenStringIssue::Unterminated;
            return finish();
        case SourceTokenStringStepKind::Pending:
            result.issue = SourceTokenStringIssue::PendingProducer;
            return finish();
        case SourceTokenStringStepKind::Invalid:
            result.issue = SourceTokenStringIssue::InvalidToken;
            return finish();
        case SourceTokenStringStepKind::OutputCapacity:
            result.issue = SourceTokenStringIssue::OutputCapacity;
            return finish();
        case SourceTokenStringStepKind::Other:
        case SourceTokenStringStepKind::LeftParenthesis:
        case SourceTokenStringStepKind::RightParenthesis:
        case SourceTokenStringStepKind::Dollar:
            break;
        }
        if (cursor.offset <= previous) {
            result.issue = SourceTokenStringIssue::InvalidToken;
            return finish();
        }
    }
}

} // namespace nijigenerate::compat
