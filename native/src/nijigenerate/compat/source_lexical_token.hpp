#pragma once

// Integration of the implemented LDC 1.41 D-mode lexical producers.
// Copyright (C) 1999-2025 The D Language Foundation (adapted components).
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_line.hpp"
#include "source_lexical_delimited_string.hpp"
#include "source_lexical_token_string.hpp"
#include "source_lexical_interpolated_string.hpp"

namespace nijigenerate::compat {

struct SourceLexicalEnvironment {
    SourceDirectiveEnvironment directive;
    std::optional<std::string_view> date;
    std::optional<std::string_view> time;
    std::optional<std::string_view> timestamp;
    std::optional<std::string_view> vendor;
};
enum class SourceAvailableTokenKind {
    EndOfInput, Symbol, Identifier, Keyword, Character, String, InterpolatedString, Integer, EnvironmentString,
    DirectiveAttempt, Invalid, InvalidToken, PendingRealProducer, PendingStringProducer,
    MissingEnvironment, OutputCapacity
};
template<std::size_t Capacity> struct SourceAvailableToken {
    SourceAvailableTokenKind kind = SourceAvailableTokenKind::InvalidToken;
    SourceLexicalTrivia lexical;
    SourceLexicalSymbolToken symbol;
    SourceLexicalIdentifierToken identifier;
    SourceCharacterLiteral character;
    SourcePlainStringLiteral<Capacity> string;
    SourceDelimitedStringLiteral<Capacity> delimitedString;
    SourceTokenStringLiteral<Capacity> tokenString;
    SourceInterpolatedStringLiteral<Capacity, Capacity + 1> interpolatedString;
    SourceIntegerLiteral integer;
    SourceLineDirective<Capacity> directive;
    SourceLiteralBytes<Capacity> environmentBytes;
    std::size_t start = 0;
    bool leadingTrivia = false;
};

// An entry for AVAILABLE producers, not a successful-token fallback for the
// remaining lexer. Pending domains leave their source start unchanged. Actual
// module/environment values must be supplied; no date, path or version is guessed.
template<std::size_t Capacity>
constexpr SourceAvailableToken<Capacity> ngScanAvailableSourceToken(std::string_view source,
        SourceLexicalPosition position = {}, SourceLexicalEnvironment environment = {},
        bool inTokenString = false) noexcept {
    SourceAvailableToken<Capacity> result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    result.start = result.lexical.position.offset;
    result.leadingTrivia = result.lexical.consumed;
    const auto at = result.lexical.position;
    if (result.lexical.issue != SourceLexicalIssue::None) { result.kind = SourceAvailableTokenKind::Invalid; return result; }
    if (result.lexical.endOfInput) { result.kind = SourceAvailableTokenKind::EndOfInput; return result; }
    if (source[result.start] == '#') {
        result.directive = ngScanSourceLineDirective<Capacity>(source, at, environment.directive, inTokenString);
        result.lexical = result.directive.lexical;
        result.kind = SourceAvailableTokenKind::DirectiveAttempt;
        return result;
    }
    result.symbol = ngScanSourceLexicalSymbol(source, at);
    if (result.symbol.kind != SourceLexicalSymbol::NotSymbol) {
        result.lexical = result.symbol.trivia;
        result.kind = result.symbol.kind == SourceLexicalSymbol::Invalid ?
            SourceAvailableTokenKind::Invalid : SourceAvailableTokenKind::Symbol;
        return result;
    }
    result.identifier = ngScanSourceLexicalIdentifier(source, at, environment.directive.identifiers);
    result.lexical = result.identifier.lexical;
    if (result.identifier.kind == SourceIdentifierKind::Invalid || result.identifier.invalidContinuation) {
        result.kind = SourceAvailableTokenKind::Invalid;
        return result;
    }
    if (result.identifier.kind == SourceIdentifierKind::Identifier || result.identifier.kind == SourceIdentifierKind::Keyword) {
        result.kind = result.identifier.kind == SourceIdentifierKind::Keyword ?
            SourceAvailableTokenKind::Keyword : SourceAvailableTokenKind::Identifier;
        return result;
    }
    if (result.identifier.kind == SourceIdentifierKind::Magic) {
        const auto magic = result.identifier.magic;
        if (magic == SourceMagicIdentifier::EndOfFile) {
            result.kind = SourceAvailableTokenKind::EndOfInput;
            return result;
        }
        if (magic == SourceMagicIdentifier::Version) {
            if (!environment.directive.compilerVersion) { result.kind = SourceAvailableTokenKind::MissingEnvironment; return result; }
            result.integer.kind = SourceIntegerKind::Int64;
            result.integer.value = *environment.directive.compilerVersion;
            result.integer.start = result.identifier.start;
            result.integer.length = result.identifier.length;
            result.integer.lexical = result.identifier.lexical;
            result.kind = SourceAvailableTokenKind::Integer;
            return result;
        }
        const auto value = magic == SourceMagicIdentifier::Date ? environment.date :
            magic == SourceMagicIdentifier::Time ? environment.time :
            magic == SourceMagicIdentifier::Timestamp ? environment.timestamp : environment.vendor;
        if (!value) { result.kind = SourceAvailableTokenKind::MissingEnvironment; return result; }
        for (const char byte : *value) {
            if (result.environmentBytes.length == Capacity) { result.kind = SourceAvailableTokenKind::OutputCapacity; return result; }
            result.environmentBytes.data[result.environmentBytes.length++] = byte;
        }
        result.kind = SourceAvailableTokenKind::EnvironmentString;
        return result;
    }
    result.character = ngScanSourceCharacterLiteral(source, at);
    if (result.character.kind != SourceCharacterKind::NotCharacter) {
        result.lexical = result.character.lexical;
        result.kind = result.character.issue != SourceCharacterIssue::None ||
            result.character.escape.issue != SourceEscapeIssue::None || result.lexical.issue != SourceLexicalIssue::None ?
            SourceAvailableTokenKind::Invalid : SourceAvailableTokenKind::Character;
        return result;
    }
    result.string = ngScanSourcePlainStringLiteral<Capacity>(source, at);
    if (result.string.kind != SourcePlainStringKind::NotPlainString) {
        result.lexical = result.string.lexical;
        result.kind = result.string.issue == SourcePlainStringIssue::OutputCapacity ? SourceAvailableTokenKind::OutputCapacity :
            result.string.issue != SourcePlainStringIssue::None || result.string.firstEscapeIssue.issue != SourceEscapeIssue::None ||
            result.lexical.issue != SourceLexicalIssue::None ? SourceAvailableTokenKind::Invalid : SourceAvailableTokenKind::String;
        return result;
    }
    result.delimitedString = ngScanSourceDelimitedStringLiteral<Capacity>(source, at, environment.directive.identifiers);
    if (result.delimitedString.recognized) {
        result.lexical = result.delimitedString.lexical;
        result.kind = result.delimitedString.issue == SourceDelimitedStringIssue::PendingDelimiterToken ?
            SourceAvailableTokenKind::PendingStringProducer :
            result.delimitedString.issue == SourceDelimitedStringIssue::OutputCapacity ?
            SourceAvailableTokenKind::OutputCapacity :
            result.delimitedString.issue != SourceDelimitedStringIssue::None ||
            result.lexical.issue != SourceLexicalIssue::None ? SourceAvailableTokenKind::Invalid :
            SourceAvailableTokenKind::String;
        if (result.kind == SourceAvailableTokenKind::PendingStringProducer) result.lexical.position = at;
        return result;
    }
    const auto nestedToken = [&](std::string_view input, SourceLexicalPosition nestedPosition,
            bool tokenStringMode) constexpr -> SourceTokenStringStep {
            const auto token = ngScanAvailableSourceToken<Capacity>(input, nestedPosition, environment,
                inTokenString || tokenStringMode);
            using Kind = SourceAvailableTokenKind;
            using Step = SourceTokenStringStepKind;
            auto kind = token.kind == Kind::EndOfInput ? Step::EndOfInput :
                token.kind == Kind::PendingRealProducer || token.kind == Kind::PendingStringProducer ||
                    token.kind == Kind::MissingEnvironment ? Step::Pending :
                token.kind == Kind::OutputCapacity ? Step::OutputCapacity :
                token.kind == Kind::Invalid || token.kind == Kind::InvalidToken ? Step::Invalid :
                token.kind == Kind::Symbol && token.symbol.kind == SourceLexicalSymbol::LeftCurly ? Step::LeftCurly :
                token.kind == Kind::Symbol && token.symbol.kind == SourceLexicalSymbol::RightCurly ? Step::RightCurly :
                token.kind == Kind::Symbol && token.symbol.kind == SourceLexicalSymbol::LeftParenthesis ? Step::LeftParenthesis :
                token.kind == Kind::Symbol && token.symbol.kind == SourceLexicalSymbol::RightParenthesis ? Step::RightParenthesis :
                token.kind == Kind::Symbol && token.symbol.kind == SourceLexicalSymbol::Dollar ? Step::Dollar :
                Step::Other;
            if (token.kind == Kind::DirectiveAttempt) {
                const auto issue = token.directive.issue;
                kind = issue == SourceDirectiveIssue::None ? Step::Other :
                    issue == SourceDirectiveIssue::MissingEnvironment || issue == SourceDirectiveIssue::MissingTokenProducer ?
                    Step::Pending : issue == SourceDirectiveIssue::OutputCapacity ? Step::OutputCapacity : Step::Invalid;
            }
            return {kind, token.lexical};
        };
    result.tokenString = ngScanSourceTokenStringLiteral<Capacity>(source, at,
        [&](std::string_view input, SourceLexicalPosition nestedPosition) constexpr {
            return nestedToken(input, nestedPosition, true);
        });
    if (result.tokenString.recognized) {
        result.lexical = result.tokenString.lexical;
        result.kind = result.tokenString.issue == SourceTokenStringIssue::PendingProducer ?
            SourceAvailableTokenKind::PendingStringProducer :
            result.tokenString.issue == SourceTokenStringIssue::OutputCapacity ? SourceAvailableTokenKind::OutputCapacity :
            result.tokenString.issue != SourceTokenStringIssue::None ? SourceAvailableTokenKind::Invalid :
            SourceAvailableTokenKind::String;
        if (result.kind == SourceAvailableTokenKind::PendingStringProducer) result.lexical.position = at;
        return result;
    }
    if (result.identifier.kind == SourceIdentifierKind::LiteralPrefix) {
        result.interpolatedString = ngScanSourceInterpolatedStringLiteral<Capacity, Capacity + 1>(source, at, nestedToken);
        if (result.interpolatedString.kind != SourceInterpolatedStringKind::NotInterpolated) {
            result.lexical = result.interpolatedString.lexical;
            result.kind = result.interpolatedString.issue == SourceInterpolatedStringIssue::PendingProducer ?
                SourceAvailableTokenKind::PendingStringProducer :
                result.interpolatedString.issue == SourceInterpolatedStringIssue::OutputCapacity ?
                SourceAvailableTokenKind::OutputCapacity :
                result.interpolatedString.issue != SourceInterpolatedStringIssue::None ||
                result.interpolatedString.firstEscapeIssue.issue != SourceEscapeIssue::None ||
                result.lexical.issue != SourceLexicalIssue::None ? SourceAvailableTokenKind::Invalid :
                SourceAvailableTokenKind::InterpolatedString;
            if (result.kind == SourceAvailableTokenKind::PendingStringProducer) result.lexical.position = at;
        } else result.kind = SourceAvailableTokenKind::PendingStringProducer;
        return result;
    }
    result.integer = ngScanSourceIntegerLiteral(source, at);
    if (result.integer.kind != SourceIntegerKind::NotInteger) {
        result.lexical = result.integer.lexical;
        result.kind = result.integer.kind == SourceIntegerKind::RequiresReal ? SourceAvailableTokenKind::PendingRealProducer :
            result.integer.issues ? SourceAvailableTokenKind::Invalid : SourceAvailableTokenKind::Integer;
        return result;
    }
    result.lexical.position = at;
    result.kind = SourceAvailableTokenKind::InvalidToken;
    return result;
}
} // namespace nijigenerate::compat
