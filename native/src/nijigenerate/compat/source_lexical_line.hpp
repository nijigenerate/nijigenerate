#pragma once

// Adapted from LDC 1.41 dmd/lexer.d D-mode special token sequences/poundLine.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_lexical_integer.hpp"
#include "source_lexical_identifiers.hpp"
#include "source_lexical_plain_string.hpp"
#include "source_lexical_character.hpp"
#include "source_lexical_symbols.hpp"
#include <optional>

namespace nijigenerate::compat {

constexpr SourceLexicalTrivia ngSkipSourceToNextLine(
        std::string_view source, SourceLexicalPosition position) noexcept {
    SourceLexicalTrivia result{position};
    auto& cursor = result.position;
    if (cursor.offset > source.size()) {
        result.issue = SourceLexicalIssue::InvalidPosition;
        result.diagnosticOffset = cursor.offset;
        return result;
    }
    while (cursor.offset < source.size()) {
        const auto byte = static_cast<unsigned char>(source[cursor.offset]);
        if (byte == 0 || byte == 0x1a) { result.endOfInput = true; return result; }
        if (byte == '\r' || byte == '\n') {
            ++cursor.offset;
            if (byte == '\r' && cursor.offset < source.size() && source[cursor.offset] == '\n') ++cursor.offset;
            ++cursor.line;
            cursor.lineStart = cursor.offset;
            result.endOfLine = true;
            return result;
        }
        if (byte >= 0x80) {
            const auto start = cursor.offset;
            const auto scalar = ngDecodeSourceLexicalScalar(source, start);
            cursor.offset = scalar.next;
            if (scalar.issue != SourceLexicalIssue::None && result.issue == SourceLexicalIssue::None) {
                result.issue = scalar.issue;
                result.diagnosticOffset = start;
            }
            if (scalar.value == 0x2028 || scalar.value == 0x2029) {
                ++cursor.line;
                cursor.lineStart = cursor.offset;
                result.endOfLine = true;
                return result;
            }
        } else ++cursor.offset;
    }
    result.endOfInput = true;
    return result;
}

struct SourceDirectiveEnvironment {
    std::optional<std::string_view> currentFilename;
    std::optional<std::uint32_t> compilerVersion;
    SourceIdentifierTable identifiers = SourceIdentifierTable::LeastRestrictive;
};
enum class SourceDirectiveIssue {
    None, LexicalFailure, PositiveIntegerExpected, LineOutOfRange, InvalidFilename,
    UnexpectedAfterFilename, UnsupportedPreprocessor, UnsupportedIf,
    MissingEnvironment, MissingTokenProducer, OutputCapacity
};
enum class SourceDirectiveSeverity { None, Error, Deprecation };
template<std::size_t Capacity> struct SourceLineDirective {
    SourceLexicalTrivia lexical;
    SourceLiteralBytes<Capacity> filename;
    SourceDirectiveIssue issue = SourceDirectiveIssue::None;
    SourceDirectiveSeverity severity = SourceDirectiveSeverity::None;
    std::size_t start = 0;
    std::size_t length = 0;
    std::size_t substitutionOffset = 0;
    std::uint64_t diagnosticInteger = 0;
    bool handled = false;
    bool hasFilename = false;
    bool applySubstitution = false;
    bool tokenizeNewlines = false;
};

// D-mode #line only; ImportC line markers are separate. The caller supplies
// genuine environment values. This returns a real substitution record rather
// than manufacturing a global location registry or replacing the module scope.
// Malformed input reports the first issue; complete compiler recovery and
// unsupported token producers remain explicit, never successful directives.
template<std::size_t Capacity>
constexpr SourceLineDirective<Capacity> ngScanSourceLineDirective(std::string_view source,
        SourceLexicalPosition position = {}, SourceDirectiveEnvironment environment = {},
        bool inTokenString = false) noexcept {
    SourceLineDirective<Capacity> result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    result.start = result.lexical.position.offset;
    if (result.lexical.issue != SourceLexicalIssue::None || result.lexical.endOfInput ||
            source[result.start] != '#') return result;
    ++result.lexical.position.offset;
    result.tokenizeNewlines = true;
    const auto finish = [&] { result.length = result.lexical.position.offset - result.start; return result; };
    const auto trivia = [&] {
        result.lexical = ngScanSourceLexicalTrivia(source, result.lexical.position, true);
        if (result.lexical.endOfLine) result.tokenizeNewlines = false;
    };
    const auto fail = [&](SourceDirectiveIssue issue, bool skipLine) {
        result.issue = issue;
        result.severity = SourceDirectiveSeverity::Error;
        if (skipLine && !result.lexical.endOfLine) {
            result.lexical = ngSkipSourceToNextLine(source, result.lexical.position);
            if (result.lexical.endOfLine) result.tokenizeNewlines = false;
        }
        return finish();
    };
    const auto readIdentifier = [&] {
        return ngScanSourceLexicalIdentifier(source, result.lexical.position, environment.identifiers);
    };
    trivia();
    if (result.lexical.issue != SourceLexicalIssue::None) return fail(SourceDirectiveIssue::LexicalFailure, false);
    if (result.lexical.endOfLine || result.lexical.endOfInput) return finish();
    const auto directive = readIdentifier();
    if (directive.kind == SourceIdentifierKind::Invalid || directive.invalidContinuation) {
        result.lexical = directive.lexical;
        return fail(SourceDirectiveIssue::LexicalFailure, false);
    }
    result.lexical = directive.lexical;
    const auto name = source.substr(directive.start, directive.length);
    if (directive.kind == SourceIdentifierKind::Identifier && name != "line") {
        result.issue = SourceDirectiveIssue::UnsupportedPreprocessor;
        result.severity = inTokenString ? SourceDirectiveSeverity::Deprecation : SourceDirectiveSeverity::Error;
        return finish();
    }
    if (directive.kind == SourceIdentifierKind::Keyword && name == "if") {
        result.issue = SourceDirectiveIssue::UnsupportedIf;
        result.severity = SourceDirectiveSeverity::Error;
        return finish();
    }
    if (directive.kind != SourceIdentifierKind::Identifier || name != "line") {
        // Preserve the actually consumed first token for arrived producers.
        if (directive.kind == SourceIdentifierKind::NotIdentifier || directive.kind == SourceIdentifierKind::LiteralPrefix) {
            const auto integer = ngScanSourceIntegerLiteral(source, result.lexical.position);
            if (integer.kind != SourceIntegerKind::NotInteger && integer.kind != SourceIntegerKind::RequiresReal)
                result.lexical = integer.lexical;
            else {
                const auto text = ngScanSourcePlainStringLiteral<Capacity>(source, result.lexical.position);
                if (text.kind != SourcePlainStringKind::NotPlainString) result.lexical = text.lexical;
                else {
                    const auto character = ngScanSourceCharacterLiteral(source, result.lexical.position);
                    if (character.kind != SourceCharacterKind::NotCharacter) result.lexical = character.lexical;
                    else {
                        const auto symbol = ngScanSourceLexicalSymbol(source, result.lexical.position);
                        result.lexical = symbol.trivia;
                        if (symbol.kind == SourceLexicalSymbol::NotSymbol || integer.kind == SourceIntegerKind::RequiresReal)
                            return fail(SourceDirectiveIssue::MissingTokenProducer, false);
                    }
                }
            }
        }
        return finish();
    }
    result.handled = true;
    trivia();
    if (result.lexical.issue != SourceLexicalIssue::None) return fail(SourceDirectiveIssue::LexicalFailure, true);
    if (result.lexical.endOfLine || result.lexical.endOfInput) return fail(SourceDirectiveIssue::PositiveIntegerExpected, false);
    const auto integer = ngScanSourceIntegerLiteral(source, result.lexical.position);
    if (integer.kind == SourceIntegerKind::Int32 || integer.kind == SourceIntegerKind::Int64) {
        result.lexical = integer.lexical;
        if (integer.issues) return fail(SourceDirectiveIssue::LexicalFailure, true);
        result.diagnosticInteger = integer.value;
        if (integer.value > 0x7fffffffull) return fail(SourceDirectiveIssue::LineOutOfRange, true);
        result.lexical.position.line = static_cast<std::size_t>(integer.value);
    } else {
        const auto argument = readIdentifier();
        result.lexical = argument.lexical;
        if (argument.kind == SourceIdentifierKind::Invalid || argument.invalidContinuation)
            return fail(SourceDirectiveIssue::LexicalFailure, true);
        const auto word = source.substr(argument.start, argument.length);
        if (argument.kind == SourceIdentifierKind::Keyword && word == "__LINE__") {
            // The actual current line counter is retained.
        } else if (argument.magic == SourceMagicIdentifier::Version) {
            if (!environment.compilerVersion) return fail(SourceDirectiveIssue::MissingEnvironment, false);
            result.diagnosticInteger = *environment.compilerVersion;
            if (*environment.compilerVersion > 0x7fffffffu) return fail(SourceDirectiveIssue::LineOutOfRange, true);
            result.lexical.position.line = *environment.compilerVersion;
        } else return fail(SourceDirectiveIssue::PositiveIntegerExpected, true);
    }
    while (true) {
        trivia();
        if (result.lexical.issue != SourceLexicalIssue::None) return fail(SourceDirectiveIssue::LexicalFailure, true);
        if (result.lexical.endOfLine || result.lexical.endOfInput) {
            result.applySubstitution = !inTokenString;
            result.substitutionOffset = result.lexical.position.offset;
            return finish();
        }
        if (result.hasFilename) return fail(SourceDirectiveIssue::UnexpectedAfterFilename, true);
        const auto at = result.lexical.position.offset;
        if (source[at] == '"') {
            const auto text = ngScanSourcePlainStringLiteral<Capacity>(source, result.lexical.position);
            result.lexical = text.lexical;
            if (text.issue == SourcePlainStringIssue::OutputCapacity) return fail(SourceDirectiveIssue::OutputCapacity, false);
            if (text.issue != SourcePlainStringIssue::None || text.lexical.issue != SourceLexicalIssue::None ||
                    text.firstEscapeIssue.issue != SourceEscapeIssue::None) return fail(SourceDirectiveIssue::LexicalFailure, true);
            if (text.postfix) return fail(SourceDirectiveIssue::InvalidFilename, true);
            result.filename = text.bytes;
            // BaseLoc consumes a C filename; preserve presence even when empty.
            for (std::size_t i = 0; i < result.filename.length; ++i)
                if (result.filename.data[i] == '\0') { result.filename.length = i; break; }
        } else {
            const auto argument = readIdentifier();
            result.lexical = argument.lexical;
            if (argument.kind != SourceIdentifierKind::Keyword || argument.invalidContinuation ||
                    source.substr(argument.start, argument.length) != "__FILE__")
                return fail(SourceDirectiveIssue::InvalidFilename, true);
            if (!environment.currentFilename) return fail(SourceDirectiveIssue::MissingEnvironment, false);
            for (const char byte : *environment.currentFilename) {
                if (!byte) break;
                if (result.filename.length == Capacity) return fail(SourceDirectiveIssue::OutputCapacity, false);
                result.filename.data[result.filename.length++] = byte;
            }
        }
        result.hasFilename = true;
    }
}
} // namespace nijigenerate::compat
