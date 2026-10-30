#pragma once

// Adapted from LDC 1.41 dmd/parse.d parseCmpExp's operator rule.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_token.hpp"

namespace nijigenerate::compat {

enum class SourceComparisonOperator {
    None, Equal, NotEqual, LessThan, LessOrEqual, GreaterThan, GreaterOrEqual,
    Identity, NotIdentity, In, NotIn
};
struct SourceComparisonFragment {
    SourceComparisonOperator kind = SourceComparisonOperator::None;
    SourceLexicalTrivia lexical;
    bool leadingSeparator = false;
};

constexpr std::string_view ngSourceComparisonOperatorSpelling(SourceComparisonOperator kind) noexcept {
    switch (kind) {
    case SourceComparisonOperator::None: return {};
    case SourceComparisonOperator::Equal: return "==";
    case SourceComparisonOperator::NotEqual: return "!=";
    case SourceComparisonOperator::LessThan: return "<";
    case SourceComparisonOperator::LessOrEqual: return "<=";
    case SourceComparisonOperator::GreaterThan: return ">";
    case SourceComparisonOperator::GreaterOrEqual: return ">=";
    case SourceComparisonOperator::Identity: return "is";
    case SourceComparisonOperator::NotIdentity: return "!is";
    case SourceComparisonOperator::In: return "in";
    case SourceComparisonOperator::NotIn: return "!in";
    }
    return {};
}

// This consumes a complete operator-text fragment before an appended right
// operand, not a complete D expression. Other operators/expressions remain for
// the actual expression parser. Raw Source text/alias identity is never changed.
template<std::size_t Capacity>
constexpr SourceComparisonFragment ngParseSourceComparisonFragment(std::string_view source) noexcept {
    SourceComparisonFragment result;
    const auto token = ngScanAvailableSourceToken<Capacity>(source);
    result.lexical = token.lexical;
    result.leadingSeparator = token.leadingTrivia;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endedInLineComment ||
            token.kind == SourceAvailableTokenKind::EndOfInput) return result;
    const auto symbol = token.kind == SourceAvailableTokenKind::Symbol ? token.symbol.kind : SourceLexicalSymbol::NotSymbol;
    switch (symbol) {
    case SourceLexicalSymbol::Equal: result.kind = SourceComparisonOperator::Equal; break;
    case SourceLexicalSymbol::NotEqual: result.kind = SourceComparisonOperator::NotEqual; break;
    case SourceLexicalSymbol::LessThan: result.kind = SourceComparisonOperator::LessThan; break;
    case SourceLexicalSymbol::LessOrEqual: result.kind = SourceComparisonOperator::LessOrEqual; break;
    case SourceLexicalSymbol::GreaterThan: result.kind = SourceComparisonOperator::GreaterThan; break;
    case SourceLexicalSymbol::GreaterOrEqual: result.kind = SourceComparisonOperator::GreaterOrEqual; break;
    case SourceLexicalSymbol::Not:
    case SourceLexicalSymbol::NotSymbol: {
        const bool negate = symbol == SourceLexicalSymbol::Not;
        const auto next = negate ? ngScanAvailableSourceToken<Capacity>(source, token.lexical.position) : token;
        const auto& word = next.identifier;
        result.lexical = word.lexical;
        if (next.kind != SourceAvailableTokenKind::Keyword || word.invalidContinuation) return result;
        const auto text = source.substr(word.start, word.length);
        if (text == "is")
            result.kind = negate ? SourceComparisonOperator::NotIdentity : SourceComparisonOperator::Identity;
        else if (text == "in")
            result.kind = negate ? SourceComparisonOperator::NotIn : SourceComparisonOperator::In;
        else return result;
        break;
    }
    default: return result;
    }
    result.lexical = ngScanSourceLexicalTrivia(source, result.lexical.position);
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endedInLineComment ||
            result.lexical.position.offset != source.size()) result.kind = SourceComparisonOperator::None;
    return result;
}
} // namespace nijigenerate::compat
