#pragma once

// Adapted from LDC 1.41 dmd/lexer.d, D-mode symbol branches.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_trivia.hpp"

namespace nijigenerate::compat {

enum class SourceLexicalSymbol {
    NotSymbol, EndOfInput, Invalid,
    LeftParenthesis, RightParenthesis, LeftBracket, RightBracket, LeftCurly, RightCurly,
    Colon, ColonColon, Semicolon, Dot, Slice, Ellipsis, Dollar, At, Question, Comma,
    LessThan, GreaterThan, LessOrEqual, GreaterOrEqual, Equal, NotEqual,
    LeftShift, RightShift, UnsignedRightShift, LeftShiftAssign, RightShiftAssign,
    UnsignedRightShiftAssign, ConcatenateAssign, Add, Subtract, AddAssign, SubtractAssign,
    Multiply, Divide, Remainder, MultiplyAssign, DivideAssign, RemainderAssign,
    BitAnd, BitOr, BitXor, AndAssign, OrAssign, XorAssign, Assign, Not, Tilde,
    Increment, Decrement, LogicalAnd, LogicalOr, Power, PowerAssign, Arrow, GoesTo
};

struct SourceLexicalSymbolToken {
    SourceLexicalSymbol kind = SourceLexicalSymbol::NotSymbol;
    std::size_t start = 0;
    std::size_t length = 0;
    SourceLexicalTrivia trivia;
};

// Scans every non-directive D punctuation token. Identifiers, literals and '#'
// belong to their separate lexer stages and remain unconsumed, not invented
// error tokens. In particular, .digit belongs to the real-literal stage.
constexpr SourceLexicalSymbolToken ngScanSourceLexicalSymbol(
        std::string_view source, SourceLexicalPosition position = {}) noexcept {
    SourceLexicalSymbolToken result;
    result.trivia = ngScanSourceLexicalTrivia(source, position);
    result.start = result.trivia.position.offset;
    if (result.trivia.issue != SourceLexicalIssue::None) {
        result.kind = SourceLexicalSymbol::Invalid;
        return result;
    }
    if (result.trivia.endOfInput) {
        result.kind = SourceLexicalSymbol::EndOfInput;
        return result;
    }
    const auto remaining = source.substr(result.start);
    if (remaining.front() == '.' && remaining.size() > 1 &&
            remaining[1] >= '0' && remaining[1] <= '9') return result;
    struct Entry { std::string_view bytes; SourceLexicalSymbol kind; };
    // Longest matches precede shorter prefixes, as in the selected scanner.
    constexpr Entry symbols[] = {
        {">>>=", SourceLexicalSymbol::UnsignedRightShiftAssign},
        {">>>", SourceLexicalSymbol::UnsignedRightShift},
        {">>=", SourceLexicalSymbol::RightShiftAssign},
        {"<<=", SourceLexicalSymbol::LeftShiftAssign},
        {"^^=", SourceLexicalSymbol::PowerAssign},
        {"...", SourceLexicalSymbol::Ellipsis},
        {"..", SourceLexicalSymbol::Slice},
        {"&=", SourceLexicalSymbol::AndAssign}, {"&&", SourceLexicalSymbol::LogicalAnd},
        {"|=", SourceLexicalSymbol::OrAssign}, {"||", SourceLexicalSymbol::LogicalOr},
        {"-=", SourceLexicalSymbol::SubtractAssign}, {"--", SourceLexicalSymbol::Decrement},
        {"->", SourceLexicalSymbol::Arrow}, {"+=", SourceLexicalSymbol::AddAssign},
        {"++", SourceLexicalSymbol::Increment}, {"<=", SourceLexicalSymbol::LessOrEqual},
        {"<<", SourceLexicalSymbol::LeftShift}, {">=", SourceLexicalSymbol::GreaterOrEqual},
        {">>", SourceLexicalSymbol::RightShift}, {"!=", SourceLexicalSymbol::NotEqual},
        {"==", SourceLexicalSymbol::Equal}, {"=>", SourceLexicalSymbol::GoesTo},
        {"~=", SourceLexicalSymbol::ConcatenateAssign}, {"^^", SourceLexicalSymbol::Power},
        {"^=", SourceLexicalSymbol::XorAssign}, {"::", SourceLexicalSymbol::ColonColon},
        {"*=", SourceLexicalSymbol::MultiplyAssign}, {"/=", SourceLexicalSymbol::DivideAssign},
        {"%=", SourceLexicalSymbol::RemainderAssign},
        {"(", SourceLexicalSymbol::LeftParenthesis}, {")", SourceLexicalSymbol::RightParenthesis},
        {"[", SourceLexicalSymbol::LeftBracket}, {"]", SourceLexicalSymbol::RightBracket},
        {"{", SourceLexicalSymbol::LeftCurly}, {"}", SourceLexicalSymbol::RightCurly},
        {":", SourceLexicalSymbol::Colon}, {";", SourceLexicalSymbol::Semicolon},
        {".", SourceLexicalSymbol::Dot}, {"$", SourceLexicalSymbol::Dollar},
        {"@", SourceLexicalSymbol::At}, {"?", SourceLexicalSymbol::Question},
        {",", SourceLexicalSymbol::Comma}, {"<", SourceLexicalSymbol::LessThan},
        {">", SourceLexicalSymbol::GreaterThan}, {"+", SourceLexicalSymbol::Add},
        {"-", SourceLexicalSymbol::Subtract}, {"*", SourceLexicalSymbol::Multiply},
        {"/", SourceLexicalSymbol::Divide}, {"%", SourceLexicalSymbol::Remainder},
        {"&", SourceLexicalSymbol::BitAnd}, {"|", SourceLexicalSymbol::BitOr},
        {"^", SourceLexicalSymbol::BitXor}, {"=", SourceLexicalSymbol::Assign},
        {"!", SourceLexicalSymbol::Not}, {"~", SourceLexicalSymbol::Tilde}
    };
    for (const auto& symbol : symbols) {
        if (!remaining.starts_with(symbol.bytes)) continue;
        result.kind = symbol.kind;
        result.length = symbol.bytes.size();
        result.trivia.position.offset += result.length;
        return result;
    }
    return result;
}
} // namespace nijigenerate::compat
