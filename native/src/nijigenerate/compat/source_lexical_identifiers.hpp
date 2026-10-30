#pragma once

// Adapted from LDC 1.41 dmd/lexer.d, common/charactertables.d and id.d.
// Copyright (C) 1999-2025 The D Language Foundation.
// BSL-1.0; see native/port/licenses/source-expression-lexical.txt.

#include "source_identifier_tables.hpp"
#include "source_lexical_trivia.hpp"

namespace nijigenerate::compat {

enum class SourceIdentifierTable { Uax31, C99, C11, LeastRestrictive };
enum class SourceIdentifierKind { NotIdentifier, Identifier, Keyword, Magic, EndOfInput, Invalid, LiteralPrefix };
enum class SourceMagicIdentifier { None, Date, Time, Timestamp, Vendor, Version, EndOfFile };

namespace detail {
template<std::size_t Count>
constexpr bool sourceIdentifierRangeContains(const SourceIdentifierRange (&ranges)[Count], char32_t value) noexcept {
    std::size_t low = 0;
    std::size_t high = Count;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        if (value < ranges[middle].first) high = middle;
        else if (value > ranges[middle].last) low = middle + 1;
        else return true;
    }
    return false;
}
constexpr bool sourceIdentifierAsciiStart(char32_t value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}
}

constexpr bool ngSourceIdentifierStart(char32_t value,
        SourceIdentifierTable table = SourceIdentifierTable::LeastRestrictive) noexcept {
    if (value < 0x80) return detail::sourceIdentifierAsciiStart(value);
    switch (table) {
    case SourceIdentifierTable::Uax31: return detail::sourceIdentifierRangeContains(detail::UAX31_Start, value);
    case SourceIdentifierTable::C99: return detail::sourceIdentifierRangeContains(detail::FixedTable_C99_Start, value);
    case SourceIdentifierTable::C11: return detail::sourceIdentifierRangeContains(detail::FixedTable_C11_Start, value);
    case SourceIdentifierTable::LeastRestrictive:
        return detail::sourceIdentifierRangeContains(detail::LeastRestrictive_Start, value);
    }
    return false;
}
constexpr bool ngSourceIdentifierContinue(char32_t value,
        SourceIdentifierTable table = SourceIdentifierTable::LeastRestrictive) noexcept {
    if (value < 0x80) return detail::sourceIdentifierAsciiStart(value) || (value >= '0' && value <= '9');
    switch (table) {
    case SourceIdentifierTable::Uax31: return detail::sourceIdentifierRangeContains(detail::UAX31_Continue, value);
    case SourceIdentifierTable::C99:
        return detail::sourceIdentifierRangeContains(detail::FixedTable_C99_Continue, value);
    case SourceIdentifierTable::C11:
        return detail::sourceIdentifierRangeContains(detail::FixedTable_C11_Continue, value);
    case SourceIdentifierTable::LeastRestrictive:
        return detail::sourceIdentifierRangeContains(detail::LeastRestrictive_Continue, value);
    }
    return false;
}
constexpr bool ngSourceAnyIdentifierCharacter(char32_t value) noexcept {
    return detail::sourceIdentifierRangeContains(detail::LeastRestrictive_OfAll, value);
}
constexpr bool ngSourceDKeyword(std::string_view word) noexcept {
    for (const auto keyword : detail::sourceDKeywords) if (word == keyword) return true;
    return false;
}
constexpr SourceMagicIdentifier ngSourceMagicIdentifier(std::string_view word) noexcept {
    if (word == "__DATE__") return SourceMagicIdentifier::Date;
    if (word == "__TIME__") return SourceMagicIdentifier::Time;
    if (word == "__TIMESTAMP__") return SourceMagicIdentifier::Timestamp;
    if (word == "__VENDOR__") return SourceMagicIdentifier::Vendor;
    if (word == "__VERSION__") return SourceMagicIdentifier::Version;
    if (word == "__EOF__") return SourceMagicIdentifier::EndOfFile;
    return SourceMagicIdentifier::None;
}

struct SourceLexicalIdentifierToken {
    SourceIdentifierKind kind = SourceIdentifierKind::NotIdentifier;
    SourceMagicIdentifier magic = SourceMagicIdentifier::None;
    std::size_t start = 0;
    std::size_t length = 0;
    SourceLexicalTrivia lexical;
    bool invalidContinuation = false;
    char32_t diagnosticScalar = 0;
    std::size_t diagnosticOffset = 0;
};

// Identifier spans and keyword identity are produced here. Magic date/time/
// vendor/version values require the real CompileEnv, not invented constants.
// Literal prefixes remain unconsumed for their actual string-literal stage.
constexpr SourceLexicalIdentifierToken ngScanSourceLexicalIdentifier(std::string_view source,
        SourceLexicalPosition position = {},
        SourceIdentifierTable table = SourceIdentifierTable::LeastRestrictive) noexcept {
    SourceLexicalIdentifierToken result;
    result.lexical = ngScanSourceLexicalTrivia(source, position);
    result.start = result.lexical.position.offset;
    if (result.lexical.issue != SourceLexicalIssue::None) { result.kind = SourceIdentifierKind::Invalid; return result; }
    if (result.lexical.endOfInput) { result.kind = SourceIdentifierKind::EndOfInput; return result; }
    const auto rest = source.substr(result.start);
    if (rest.starts_with("r\"") || rest.starts_with("x\"") || rest.starts_with("q\"") ||
            rest.starts_with("q{") || rest.starts_with("i\"") || rest.starts_with("i`") || rest.starts_with("iq{")) {
        result.kind = SourceIdentifierKind::LiteralPrefix;
        return result;
    }
    auto& cursor = result.lexical.position;
    const auto first = ngDecodeSourceLexicalScalar(source, cursor.offset);
    if (first.issue != SourceLexicalIssue::None) {
        result.kind = SourceIdentifierKind::Invalid;
        result.lexical.issue = first.issue;
        result.lexical.diagnosticOffset = cursor.offset;
        return result;
    }
    if (!ngSourceIdentifierStart(first.value, table)) return result;
    cursor.offset = first.next;
    while (cursor.offset < source.size()) {
        const auto byte = static_cast<unsigned char>(source[cursor.offset]);
        if (byte < 0x80) {
            if (!ngSourceIdentifierContinue(byte, table)) break;
            ++cursor.offset;
            continue;
        }
        const auto scalar = ngDecodeSourceLexicalScalar(source, cursor.offset);
        if (scalar.issue != SourceLexicalIssue::None) {
            result.kind = SourceIdentifierKind::Invalid;
            result.lexical.issue = scalar.issue;
            result.lexical.diagnosticOffset = cursor.offset;
            return result;
        }
        if (!ngSourceIdentifierContinue(scalar.value, table)) {
            // The selected IdentLoop diagnoses this before returning its prefix
            // token, even when the scalar is a Unicode line separator.
            result.invalidContinuation = true;
            result.diagnosticScalar = scalar.value;
            result.diagnosticOffset = cursor.offset;
            break;
        }
        cursor.offset = scalar.next;
    }
    result.length = cursor.offset - result.start;
    const auto word = source.substr(result.start, result.length);
    result.magic = ngSourceMagicIdentifier(word);
    if (result.magic != SourceMagicIdentifier::None) {
        result.kind = SourceIdentifierKind::Magic;
        if (result.magic == SourceMagicIdentifier::EndOfFile && !result.invalidContinuation) {
            // Source __EOF__ skips bytes, without lexing or updating line state.
            while (cursor.offset < source.size() && source[cursor.offset] != '\0' &&
                    static_cast<unsigned char>(source[cursor.offset]) != 0x1a) ++cursor.offset;
        }
    } else result.kind = ngSourceDKeyword(word) ? SourceIdentifierKind::Keyword : SourceIdentifierKind::Identifier;
    return result;
}
} // namespace nijigenerate::compat
