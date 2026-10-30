#include "tokenizer.hpp"
#include "phobos_sort.hpp"
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_allocator.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

#include <algorithm>
#include <bit>
#include <type_traits>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <string>
#include <utility>

namespace nijigenerate::core::selector {
namespace {
template<class Operation> auto allocateString(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString comparisonMessage(compat::RetainedString name) {
    return allocateString([&] {
        std::string bytes = "need opCmp for class ";
        bytes += name.view();
        return compat::RetainedString(std::move(bytes));
    });
}
template<class Output> void appendToken(Output& output, Token token) {
    if constexpr (std::is_same_v<Output, std::vector<Token>>)
        allocateString([&] { output.push_back(std::move(token)); });
    else output.push_back(std::move(token));
}
}
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void Tokenizer::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Tokenizer),
            compat::RetainedString::fromStaticLiteral("nijigenerate.core.selector.tokenizer.Tokenizer"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString Tokenizer::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Tokenizer::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Tokenizer::opEquals(std::shared_ptr<Tokenizer> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int Tokenizer::opCmp(std::shared_ptr<Tokenizer>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void Scanner::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Scanner),
            compat::RetainedString::fromStaticLiteral("nijigenerate.core.selector.tokenizer.Scanner"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString Scanner::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Scanner::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Scanner::opEquals(std::shared_ptr<Scanner> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int Scanner::opCmp(std::shared_ptr<Scanner>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
namespace {
struct UnicodeRange { char32_t first; char32_t last; };
#include "unicode_ranges.inc"

template<std::size_t N>
bool contains(const UnicodeRange (&ranges)[N], char32_t c) {
    const auto it = std::lower_bound(std::begin(ranges), std::end(ranges), c,
        [](const UnicodeRange& range, char32_t value) { return range.last < value; });
    return it != std::end(ranges) && it->first <= c;
}
bool isAlpha(char32_t c) { return contains(alphaRanges, c); }
bool isAlphaNum(char32_t c) { return contains(alphaNumRanges, c); }
bool isNumber(char32_t c) { return contains(numberRanges, c); }
bool isSpace(char32_t c) { return contains(spaceRanges, c); }

std::size_t stride(char32_t c) {
    return c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
}

char32_t decode(std::string_view text, std::size_t& position) {
    if (position >= text.size()) {
#ifndef NDEBUG
        throw compat::ngStaticError<compat::AssertError>(compat::RetainedString::fromStaticLiteral(
            "Attempted to decode past the end of a string"),
            compat::RetainedString::fromStaticLiteral("std/utf.d"), 1175);
#else
        throw compat::ngStaticError<compat::ArrayIndexError>(position, text.size(),
            compat::RetainedString::fromStaticLiteral("std/utf.d"), 1183);
#endif
    }
    // This is the actual shared Phobos decoder, including the copied invalid
    // sequence and its source index. Advance the caller only after success.
    using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view, char>;
    const auto result = Cursor(text.substr(position)).front();
    position += stride(result);
    return result;
}

void append(std::string& output, char32_t c) {
    allocateString([&] {
        if (c < 0x80) output += static_cast<char>(c);
        else if (c < 0x800) {
            output += static_cast<char>(0xc0 | (c >> 6));
            output += static_cast<char>(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            output += static_cast<char>(0xe0 | (c >> 12));
            output += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            output += static_cast<char>(0x80 | (c & 0x3f));
        } else {
            output += static_cast<char>(0xf0 | (c >> 18));
            output += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
            output += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            output += static_cast<char>(0x80 | (c & 0x3f));
        }
    });
}
}

using RetainedText = nijigenerate::compat::RetainedStringSlice;
char32_t ngDecodeSelectorUtf8(std::string_view text, std::size_t& position) {
    return decode(text, position);
}
using TextOwner = nijigenerate::compat::RetainedString;

namespace {
// The original constructor retains literal storage across Tokenizer instances.
template<std::size_t N> Token reservedToken(Token::Type type, const char (&text)[N]) {
    return Token(type, RetainedText(TextOwner::fromStaticLiteral(text)));
}
}

Token::Token(Type tokenType) : type(tokenType), literal(TextOwner::emptyLiteral()) {}
Token::Token(Type tokenType, RetainedText tokenLiteral) : type(tokenType), literal(tokenLiteral) {}
Token::Token(Type tokenType, std::string tokenLiteral)
    : Token(tokenType, allocateString([&] { return RetainedText(TextOwner(std::move(tokenLiteral))); })) {}
Token::Token(Type tokenType, const char* tokenLiteral)
    : Token(tokenType, allocateString([&] { return RetainedText(TextOwner(tokenLiteral)); })) {}
bool Token::match(std::string_view text) const { return text.starts_with(literal.view()); }
RetainedText Token::toString() const {
    if (literal.empty()) {
        // Each D literal has stable storage across repeated calls.
        static const RetainedText identifier(TextOwner::fromStaticLiteral("<ID>"));
        static const RetainedText digits(TextOwner::fromStaticLiteral("<##>"));
        static const RetainedText string(TextOwner::fromStaticLiteral("<TEXT>"));
        if (type == Type::Identifier) return identifier;
        if (type == Type::Digits) return digits;
        if (type == Type::String) return string;
    }
    return literal;
}
bool Token::opEquals(const Token& rhs) const {
    if (type == Type::Identifier || type == Type::Digits || type == Type::String)
        return type == rhs.type && (literal.empty() || literal == rhs.literal);
    return type == rhs.type;
}
bool Token::equals(Type rhs) const { return type == rhs; }
bool Token::equals(std::string_view rhs) const { return literal.view() == rhs; }

Tokenizer::Tokenizer() : reservedWord {
    reservedToken(Token::Type::Equals, "=="), reservedToken(Token::Type::NotEquals, "!="),
    reservedToken(Token::Type::GreaterEq, ">="), reservedToken(Token::Type::LessEq, "<="),
    reservedToken(Token::Type::Greater, ">"), reservedToken(Token::Type::Less, "<"),
    reservedToken(Token::Type::Assign, "="), reservedToken(Token::Type::LLParen, "["),
    reservedToken(Token::Type::RLParen, "]"), reservedToken(Token::Type::LMParen, "{"),
    reservedToken(Token::Type::RMParen, "}"), reservedToken(Token::Type::LSParen, "("),
    reservedToken(Token::Type::RSParen, ")"), reservedToken(Token::Type::Comma, ","),
    reservedToken(Token::Type::Dot, "."), reservedToken(Token::Type::Colon, ":"),
    reservedToken(Token::Type::Plus, "+"), reservedToken(Token::Type::Minus, "-"),
    reservedToken(Token::Type::Multiply, "*"), reservedToken(Token::Type::Division, "/"),
    reservedToken(Token::Type::Sharp, "#"), reservedToken(Token::Type::Hash, "|"),
    reservedToken(Token::Type::Dollar, "$"), reservedToken(Token::Type::Question, "?")
} {
    for (auto& token : reservedWord)
        reservedDict[token.literal.retainedString()] = std::shared_ptr<Token>(reservedWord.owner(), &token);
}

void Tokenizer::build() {
    std::int32_t numInternalState = -1;
    detail::phobosTokenSort(std::span<Token>(reservedWord),
        [](const Token& a, const Token& b) { return a.literal.size() < b.literal.size(); });
    const auto words = reservedWord;
    for (auto token : words) {
        std::uint64_t curLen = 0;
        const auto tokenLen = static_cast<std::uint64_t>(token.literal.size());
        std::int32_t internalState = -1;
        (void)reservedWordMap[internalState];
        // D foreach on a string with an inferred element type iterates bytes.
        for (const unsigned char next : token.literal.view()) {
            ++curLen;
            auto row = reservedWordMap[internalState];
            auto& transitions = static_cast<Transitions&>(row);
            if (!transitions.contains(next)) {
                if (curLen == tokenLen) {
                    transitions[next] = static_cast<std::int32_t>(token.type);
                } else {
                    numInternalState = std::bit_cast<std::int32_t>(
                        static_cast<std::uint32_t>(numInternalState) - std::uint32_t{1});
                    transitions[next] = numInternalState;
                }
                internalState = *transitions.findRetained(next);
                (void)reservedWordMap[internalState];
            } else internalState = *transitions.findRetained(next);
        }
    }
    detail::phobosTokenSort(std::span<Token>(reservedWord),
        [](const Token& a, const Token& b) { return a.literal.size() > b.literal.size(); });
}

template<class Output>
void Tokenizer::tokenizeImpl(Tokenizer* receiver, bool useMap, RetainedText input, std::size_t position,
                           Output& tokens, std::size_t& nextPosition, bool skipSpace) {
    const auto text = input.view();
    // D out initializes the descriptor, rather than clearing storage shared by old aliases.
    if constexpr (std::is_same_v<Output, SelectorArray<Token>>) tokens = SelectorArray<Token>{};
    else tokens.clear();
    nextPosition = 0;
    auto i = position;
    if (useMap && compat::ngRequireObject(receiver)->reservedWordMap.empty()) receiver->build();
    while (i < text.size()) {
        if (!useMap) {
            bool found = false;
            const auto words = compat::ngRequireObject(receiver)->reservedWord;
            for (auto token : words) {
                if (token.match(text.substr(i))) {
                    appendToken(tokens, token);
                    i += token.literal.size();
                    found = true;
                    break;
                }
            }
            if (found) continue;
        } else {
            std::int32_t state = -1;
            std::int32_t size = 0;
            auto type = Token::Type::Invalid;
            for (const unsigned char nextChar : text.substr(i)) {
                const auto transitions = compat::ngRequireObject(receiver)->reservedWordMap.findRetained(state);
                if (!transitions) throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/selector/tokenizer.d"), 190);
                const auto found = transitions->findRetained(nextChar);
                if (!found) break;
                if (*found >= 0) { type = static_cast<Token::Type>(*found); state = *found; }
                else state = *found;
                size = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(size) + std::uint32_t{1});
            }
            if (type != Token::Type::Invalid) {
                const auto end = i + static_cast<std::size_t>(size);
                if (i > end || end > input.size()) throw compat::ngStaticError<compat::ArraySliceError>(
                    i, end, input.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/selector/tokenizer.d"), 201);
                appendToken(tokens, Token(type, input.slice(i, end)));
                i += static_cast<std::size_t>(size);
                continue;
            }
        }
        const auto head = decode(text, i);
        if (isAlpha(head)) {
            std::string literal;
            append(literal, head);
            if (i < text.size()) {
                auto next = decode(text, i);
                while (true) {
                    if (!isAlphaNum(next) && next != '-' && next != '_') { i -= stride(next); break; }
                    append(literal, next);
                    if (i >= text.size()) break;
                    next = decode(text, i);
                }
            }
            appendToken(tokens, Token(Token::Type::Identifier, std::move(literal)));
            continue;
        }
        if (isNumber(head)) {
            std::string literal;
            append(literal, head);
            if (i < text.size()) {
                auto next = decode(text, i);
                while (true) {
                    if (!isNumber(next)) break;
                    append(literal, next);
                    if (i >= text.size()) break;
                    next = decode(text, i);
                }
                if (next == '.') {
                    append(literal, '.');
                    next = decode(text, i);
                    while (true) {
                        if (!isNumber(next)) { i -= stride(next); break; }
                        append(literal, next);
                        if (i >= text.size()) break;
                        next = decode(text, i);
                    }
                } else i -= stride(next);
            }
            appendToken(tokens, Token(Token::Type::Digits, std::move(literal)));
            continue;
        }
        if (isSpace(head)) {
            std::string literal;
            append(literal, head);
            if (i < text.size()) {
                auto next = decode(text, i);
                while (true) {
                    if (!isSpace(next)) { i -= stride(next); break; }
                    append(literal, next);
                    if (i >= text.size()) break;
                    next = decode(text, i);
                }
            }
            if (!skipSpace) appendToken(tokens, Token(Token::Type::Identifier, std::move(literal)));
            continue;
        }
        if (head == '"') {
            std::string literal;
            if (i < text.size()) {
                auto next = decode(text, i);
                while (next != '"') {
                    append(literal, next);
                    if (i >= text.size()) break;
                    next = decode(text, i);
                }
                i -= stride(next);
                // D starts the quoted accumulator as null; no append keeps it null.
                appendToken(tokens, Token(Token::Type::String, literal.empty() ? RetainedText{} :
                    allocateString([&] { return RetainedText(TextOwner(std::move(literal))); })));
                ++i;
            }
            continue;
        }
    }
    nextPosition = i;
}

template void Tokenizer::tokenizeImpl<SelectorArray<Token>>(Tokenizer*, bool, RetainedText, std::size_t,
    SelectorArray<Token>&, std::size_t&, bool);
template void Tokenizer::tokenizeImpl<std::vector<Token>>(Tokenizer*, bool, RetainedText, std::size_t,
    std::vector<Token>&, std::size_t&, bool);

Scanner::Scanner(SelectorArray<Token> values) : tokens(std::move(values)) {}
Scanner::Scanner(std::shared_ptr<std::vector<Token>> values) : Scanner(SelectorArray<Token>(std::move(values))) {}
Scanner::Scanner(std::vector<Token> values) : Scanner(SelectorArray<Token>(std::move(values))) {}
Token Scanner::scan() {
    const auto position = static_cast<std::size_t>(index);
    if (position < tokens.size()) {
        // Preserve the source int32 post-increment without C++ signed overflow.
        index = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index) + std::uint32_t{1});
        return tokens[position];
    }
    return Token(Token::Type::Invalid);
}
void Scanner::rewind(std::int32_t num) {
    const auto difference = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index) - static_cast<std::uint32_t>(num));
    index = difference >= 0 ? difference : 0;
}
std::shared_ptr<Scanner> Scanner::dup() {
    auto scanner = std::allocate_shared<Scanner>(nicxlive::core::d_runtime::SourceAllocator<Scanner>{}, tokens);
    scanner->index = index;
    return scanner;
}
bool Scanner::isEnd() { return static_cast<std::size_t>(index) >= tokens.size(); }

}
