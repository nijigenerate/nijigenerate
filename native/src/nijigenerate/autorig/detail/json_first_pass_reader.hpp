#pragma once

// Phobos std.json reader used by the actual application JSON consumers.
// Copyright Jeremie Pelletier 2008-2009. BSL-1.0: phobos-rig-json.txt.
// UTF range operations follow std.utf, Sean Kelly 2005-2013, under BSL-1.0.
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/phobos_unicode_exception.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include "json_numeric_first_pass.hpp"
#include <bit>
#include <array>
#include <cstdint>
#include <concepts>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::autorig::json::detail {

// Distinguish the source UTF decoding failure from a parser JSONException.
using ArtifactUtfError = ::nijigenerate::compat::UTFException;

enum class JSONOptions : std::uint32_t {
    none = 0,
    specialFloatLiterals = 0x1,
    escapeNonAsciiChars = 0x2,
    doNotEscapeSlashes = 0x4,
    strictParsing = 0x8,
    preserveObjectOrder = 0x16
};

constexpr JSONOptions operator|(JSONOptions left, JSONOptions right) noexcept {
    return static_cast<JSONOptions>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}
constexpr JSONOptions operator&(JSONOptions left, JSONOptions right) noexcept {
    return static_cast<JSONOptions>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}
constexpr JSONOptions operator^(JSONOptions left, JSONOptions right) noexcept {
    return static_cast<JSONOptions>(static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
}
constexpr JSONOptions operator~(JSONOptions value) noexcept {
    return static_cast<JSONOptions>(~static_cast<std::uint32_t>(value));
}

template<class T> concept JsonCharacter =
    std::same_as<std::remove_cvref_t<T>, char> || std::same_as<std::remove_cvref_t<T>, char8_t> ||
    std::same_as<std::remove_cvref_t<T>, char16_t> || std::same_as<std::remove_cvref_t<T>, char32_t> ||
    std::same_as<std::remove_cvref_t<T>, wchar_t>;

template<class T> inline constexpr bool jsonStaticallyInfinite = [] {
    using Range = std::remove_cvref_t<T>;
    if constexpr (requires { std::bool_constant<Range::empty()>{}; }) return !Range::empty();
    else return false;
}();

// The actual translated finite range is copied by the public parse operation.
// Reading its front/popFront does not materialize another input string.
template<class T> concept JsonCharacterRange = !jsonStaticallyInfinite<T> && requires(T& input) {
    { input.empty() } -> std::convertible_to<bool>;
    requires JsonCharacter<decltype(input.front())>;
    input.popFront();
};
template<class T> concept JsonCharacterObjectRange = requires(T& input) {
    static_cast<bool>(input);
    requires !jsonStaticallyInfinite<decltype(*input)>;
    { input->empty() } -> std::convertible_to<bool>;
    requires JsonCharacter<decltype(input->front())>;
    input->popFront();
};
template<class T> concept JsonCharacterSlice = !jsonStaticallyInfinite<T> && requires(T& input) {
    requires std::is_pointer_v<decltype(input.data())>;
    requires JsonCharacter<std::remove_pointer_t<decltype(input.data())>>;
    { input.size() } -> std::convertible_to<std::size_t>;
};
template<class T> concept JsonInput =
    JsonCharacterRange<T> || JsonCharacterObjectRange<T> || JsonCharacterSlice<T>;

template<class T, bool = JsonCharacterRange<T>, bool = JsonCharacterObjectRange<T>> struct JsonInputCharacter;
template<class T, bool Object> struct JsonInputCharacter<T, true, Object> {
    using Type = std::remove_cvref_t<decltype(std::declval<T&>().front())>;
};
template<class T> struct JsonInputCharacter<T, false, true> {
    using Type = std::remove_cvref_t<decltype(std::declval<T&>()->front())>;
};
template<class T> struct JsonInputCharacter<T, false, false> {
    using Type = std::remove_cv_t<std::remove_pointer_t<decltype(std::declval<T&>().data())>>;
};

template<JsonInput Input> class JsonReader {
    using Character = typename JsonInputCharacter<Input>::Type;
    static constexpr bool byteInput = sizeof(Character) == 1;
#ifdef NG_NO_AUTODECODE_STRINGS
    static constexpr bool autoDecodeWide = false;
#else
    static constexpr bool autoDecodeWide = true;
#endif
public:
    explicit JsonReader(Input text, std::int32_t maxDepth = -1,
            JSONOptions options = JSONOptions::none)
        : text_(std::forward<Input>(text)), maxDepth_(maxDepth), options_(options) {}
    JsonReader(Input text, JSONOptions options)
        : JsonReader(std::forward<Input>(text), -1, options) {}
    bool remainingEmpty() {
        if constexpr (JsonCharacterRange<Input>) return text_.empty();
        else if constexpr (JsonCharacterObjectRange<Input>) return text_->empty();
        else return cursor_ == text_.size();
    }
    bool remainingEmpty() const noexcept
        requires std::same_as<std::remove_cvref_t<Input>, compat::RetainedStringSlice> {
        return cursor_ == text_.size();
    }

    core::settings::SettingsJsonValue parseRoot() {
        core::settings::SettingsJsonValue value;
        if (remainingEmpty()) {
            if (strict()) error("Empty JSON body");
            return value;
        }
        parseValue(value);
        if (strict()) {
            skipWhitespace();
            if (peekCharNullable()) error("Trailing non-whitespace characters");
        }
        return value;
    }

    void parseValue(core::settings::SettingsJsonValue& value) {
        depth_ = increment(depth_);
        if (maxDepth_ != -1 && depth_ > maxDepth_) error("Nesting too deep.");
        auto c = getChar(true);
        if (c == '{') {
            const bool ordered = hasOption(JSONOptions::preserveObjectOrder);
            if (testChar('}')) {
                value = core::settings::SettingsJsonObject(ordered);
                depth_ = decrement(depth_);
                return;
            }
            core::settings::SettingsJsonObject members(ordered);
            do {
                skipWhitespace();
                if (!strict() && peekChar() == '}') break;
                checkChar('"');
                auto name = parseString();
                checkChar(':');
                core::settings::SettingsJsonValue member;
                parseValue(member);
                if (ordered) members.appendOrderedMember(std::move(name), std::move(member));
                else members[std::move(name)] = std::move(member);
            } while (testChar(','));
            value = members;
            checkChar('}');
        } else if (c == '[') {
            if (testChar(']')) {
                value = core::settings::SettingsJsonArray{};
                depth_ = decrement(depth_);
                return;
            }
            core::settings::SettingsJsonArray elements;
            do {
                skipWhitespace();
                if (!strict() && peekChar() == ']') break;
                core::settings::SettingsJsonValue element;
                parseValue(element);
                elements.append(std::move(element));
            } while (testChar(','));
            checkChar(']');
            value = elements;
        } else if (c == '"') {
            auto text = parseString();
            if (hasOption(JSONOptions::specialFloatLiterals) && text.view() == "NaN")
                value = std::numeric_limits<double>::quiet_NaN();
            else if (hasOption(JSONOptions::specialFloatLiterals) && text.view() == "Infinite")
                value = std::numeric_limits<double>::infinity();
            else if (hasOption(JSONOptions::specialFloatLiterals) && text.view() == "-Infinite")
                value = -std::numeric_limits<double>::infinity();
            else value = std::move(text);
        } else if ((c >= '0' && c <= '9') || c == '-') {
            std::string number;
            bool floating = false, negative = false;
            const auto integer = [&] {
                if (!(c >= '0' && c <= '9')) error("Digit expected");
                for (;;) {
                    number += static_cast<char>(c);
                    const auto next = peekChar();
                    if (!(next >= '0' && next <= '9')) break;
                    c = getChar();
                }
            };
            if (c == '-') { number += '-'; c = getChar(); negative = true; }
            if (strict() && c == '0') {
                number += '0';
                const auto next = peekChar();
                if (next >= '0' && next <= '9')
                    error("Additional digits not allowed after initial zero digit");
            } else integer();
            if (testChar('.')) { floating = true; number += '.'; c = getChar(); integer(); }
            if (testChar('e', false, false)) {
                floating = true;
                number += 'e';
                if (testChar('+')) number += '+';
                else if (testChar('-')) number += '-';
                c = getChar();
                integer();
            }
            if (floating) {
                value = 0.0;
                value = firstPassDouble(number);
            } else {
                const auto magnitude = firstPassInteger(number, negative);
                if (negative) value = std::bit_cast<std::int64_t>(std::uint64_t{0} - magnitude);
                else if (magnitude >> 63) value = magnitude;
                else value = static_cast<std::int64_t>(magnitude);
            }
        } else if (c == 't' || (!strict() && c == 'T')) {
            value = true;
            checkChar('r', false, strict()); checkChar('u', false, strict()); checkChar('e', false, strict());
        } else if (c == 'f' || (!strict() && c == 'F')) {
            value = false;
            checkChar('a', false, strict()); checkChar('l', false, strict());
            checkChar('s', false, strict()); checkChar('e', false, strict());
        } else if (c == 'n' || (!strict() && c == 'N')) {
            value = nullptr;
            checkChar('u', false, strict()); checkChar('l', false, strict()); checkChar('l', false, strict());
        } else error(std::string("Unexpected character '") + characterText(c) + "'.");
        depth_ = decrement(depth_);
    }

    [[noreturn]] void error(const std::string& message) const {
        throw core::settings::SettingsJsonError(message, line_, position_);
    }
    std::uint32_t popChar() {
        if (remainingEmpty()) error("Unexpected end of data.");
        std::uint32_t character;
        if constexpr (JsonCharacterRange<Input>) {
            if constexpr (byteInput) character = static_cast<unsigned char>(text_.front());
            else character = static_cast<std::uint32_t>(text_.front());
            text_.popFront();
        } else if constexpr (JsonCharacterObjectRange<Input>) {
            if constexpr (byteInput) character = static_cast<unsigned char>(text_->front());
            else character = static_cast<std::uint32_t>(text_->front());
            text_->popFront();
        } else if constexpr (sizeof(Character) == 2 && autoDecodeWide) {
            // Built-in wchar ranges decode front before advancing popFront.
            const auto input = std::basic_string_view<Character>(text_.data() + cursor_, text_.size() - cursor_);
            const compat::phobos::detail::UnicodeCursor<decltype(input), Character> cursor(input);
            character = static_cast<std::uint32_t>(cursor.front());
            const auto first = static_cast<std::uint32_t>(input[0]);
            cursor_ += 1 + (first >= 0xd800 && first <= 0xdbff);
        } else {
            if constexpr (byteInput) character = static_cast<unsigned char>(text_.data()[cursor_++]);
            else character = static_cast<std::uint32_t>(text_.data()[cursor_++]);
        }
        if (character == '\n') { line_ = increment(line_); position_ = 0; }
        else position_ = increment(position_);
        return character;
    }
    std::uint32_t peekChar() {
        if (!next_) {
            if (remainingEmpty()) return 0;
            next_ = popChar();
        }
        return *next_;
    }
    std::optional<std::uint32_t> peekCharNullable() {
        if (!next_ && !remainingEmpty()) next_ = popChar();
        return next_;
    }
    void skipWhitespace() {
        for (;;) {
            const auto byte = peekCharNullable();
            if (!byte || !isWhite(*byte)) return;
            next_.reset();
        }
    }
    std::uint32_t getChar(bool whitespace = false) {
        if (whitespace) skipWhitespace();
        if (!next_) return popChar();
        const auto byte = *next_;
        next_.reset();
        return byte;
    }
    void checkChar(unsigned char expected, bool whitespace = true, bool sensitive = true) {
        if (whitespace) skipWhitespace();
        auto byte = getChar();
        if (!sensitive) byte = lower(byte);
        if (byte != expected)
            error(std::string("Found '") + characterText(byte) + "' when expecting '" +
                static_cast<char>(expected) + "'.");
    }
    bool testChar(unsigned char expected, bool whitespace = true, bool sensitive = true) {
        if (whitespace) skipWhitespace();
        auto byte = peekChar();
        if (!sensitive) byte = lower(byte);
        if (byte != expected) return false;
        getChar();
        return true;
    }
    std::uint16_t parseWChar() {
        std::uint16_t value = 0;
        for (int i = 3; i >= 0; --i) {
            auto byte = getChar();
            if (byte >= 'a' && byte <= 'z') byte -= 'a' - 'A';
            if (!((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'F')))
                error("Expecting hex character");
            value = static_cast<std::uint16_t>(value +
                ((byte <= '9' ? byte - '0' : byte - ('A' - 10)) << (4 * i)));
        }
        return value;
    }
    // The opening quote was consumed by parseValue/checkChar, as in Phobos.
    core::settings::SettingsString parseString() {
        std::string result;
        for (;;) {
            const auto next = peekChar();
            if (next == '"') { getChar(); break; }
            if (next != '\\') {
                const auto byte = getChar();
                if (byte < 0x20 && (strict() || byte != 0)) error("Illegal control character.");
                appendCharacter(result, byte);
                continue;
            }
            getChar();
            const auto escape = getChar();
            switch (escape) {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case '/': result += '/'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'u': {
                const auto first = parseWChar();
                std::uint32_t scalar = first;
                if (first >= 0xd800 && first <= 0xdbff) {
                    if (getChar() != '\\') error("Expected escaped low surrogate after escaped high surrogate");
                    if (getChar() != 'u') error("Expected escaped low surrogate after escaped high surrogate");
                    const auto second = parseWChar();
                    const char16_t pair[]{static_cast<char16_t>(first), static_cast<char16_t>(second)};
                    const std::u16string_view input(pair, 2);
                    const compat::phobos::detail::UnicodeCursor<decltype(input), char16_t> cursor(input);
                    scalar = static_cast<std::uint32_t>(cursor.front());
                } else if (first >= 0xdc00 && first <= 0xdfff) error("Unexpected low surrogate");
                appendUtf8(result, scalar);
                break;
            }
            default: error(std::string("Invalid escape sequence '\\") + characterText(escape) + "'.");
            }
        }
        return result.empty() ? core::settings::SettingsString::emptyLiteral()
            : core::settings::SettingsString(std::move(result));
    }

private:
    Input text_;
    std::size_t cursor_ = 0;
    std::optional<std::uint32_t> next_;
    std::int32_t line_ = 1;
    std::int32_t position_ = 0;
    std::int32_t depth_ = -1;
    std::int32_t maxDepth_ = -1;
    JSONOptions options_ = JSONOptions::none;
    bool hasOption(JSONOptions option) const noexcept {
        return (static_cast<std::uint32_t>(options_) & static_cast<std::uint32_t>(option)) != 0;
    }
    bool strict() const noexcept { return hasOption(JSONOptions::strictParsing); }
    static std::int32_t increment(std::int32_t value) noexcept {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1);
    }
    static std::int32_t decrement(std::int32_t value) noexcept {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) - 1);
    }
    bool isWhite(std::uint32_t c) const noexcept {
        if (strict()) return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        return c == 0 || c == ' ' || (c >= '\t' && c <= '\r');
    }
    static std::uint32_t lower(std::uint32_t c) noexcept {
        return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    }
    template<std::size_t N> [[noreturn]] static void utfFailure(const char (&message)[N],
            std::uint32_t scalar, std::size_t line) {
        ArtifactUtfError failure(compat::RetainedString::fromStaticLiteral(message),
            compat::RetainedString::fromStaticLiteral("std/utf.d"), line);
        const std::array sequence{scalar};
        failure.setSequence(sequence);
        throw failure;
    }
    static void appendCharacter(std::string& output, std::uint32_t character) {
        if constexpr (byteInput) output += static_cast<char>(character);
        else {
            if (character >= 0xd800 && character <= 0xdfff)
                utfFailure("Encoding a surrogate code point in UTF-8", character, 2311);
            if (character > 0x10ffff)
                utfFailure("Encoding an invalid code point in UTF-8", character, 2311);
            appendUtf8(output, character);
        }
    }
    static std::string characterText(std::uint32_t character) {
        std::string text;
        appendCharacter(text, character);
        return text;
    }
    static void appendUtf8(std::string& output, std::uint32_t scalar) {
        if (scalar < 0x80) output += static_cast<char>(scalar);
        else if (scalar < 0x800) {
            output += static_cast<char>(0xc0 | (scalar >> 6));
            output += static_cast<char>(0x80 | (scalar & 63));
        } else if (scalar < 0x10000) {
            output += static_cast<char>(0xe0 | (scalar >> 12));
            output += static_cast<char>(0x80 | ((scalar >> 6) & 63));
            output += static_cast<char>(0x80 | (scalar & 63));
        } else {
            output += static_cast<char>(0xf0 | (scalar >> 18));
            output += static_cast<char>(0x80 | ((scalar >> 12) & 63));
            output += static_cast<char>(0x80 | ((scalar >> 6) & 63));
            output += static_cast<char>(0x80 | (scalar & 63));
        }
    }
};

using FirstPassReader = JsonReader<compat::RetainedStringSlice>;

// Keep the original identifier so public source aliases import this exact
// overload family, rather than another parser or an options-rejecting wrapper.
template<JsonInput Input>
core::settings::SettingsJsonValue parseJSON(Input input, std::int32_t maxDepth = -1,
        JSONOptions options = JSONOptions::none) {
    return JsonReader<Input&>(input, maxDepth, options).parseRoot();
}

template<JsonInput Input>
core::settings::SettingsJsonValue parseJSON(Input input, JSONOptions options) {
    return ::nijigenerate::autorig::json::detail::parseJSON<Input>(input, -1, options);
}

template<JsonCharacter Character, std::size_t N>
core::settings::SettingsJsonValue parseJSON(const Character (&input)[N], std::int32_t maxDepth = -1,
        JSONOptions options = JSONOptions::none) {
    static_assert(N > 0);
    auto text = std::basic_string_view<Character>(input, N - 1);
    return JsonReader<decltype(text)&>(text, maxDepth, options).parseRoot();
}

template<JsonCharacter Character, std::size_t N>
core::settings::SettingsJsonValue parseJSON(const Character (&input)[N], JSONOptions options) {
    return ::nijigenerate::autorig::json::detail::parseJSON<Character, N>(input, -1, options);
}

} // namespace nijigenerate::autorig::json::detail
