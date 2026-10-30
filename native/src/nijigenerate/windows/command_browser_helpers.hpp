#pragma once
#include "command_browser_resources.hpp"
#include "command_browser_float_parse.hpp"
#include <nijigenerate/io/depthmap_psd_names.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <limits>
#include <cstdint>
#include <type_traits>
#include <utility>
namespace nijigenerate::windows::command_browser::detail {
template<class E> struct BrowserEnumTraits;
template<class T> inline constexpr bool isBrowserEnum = std::is_enum_v<T> || requires(const T& value) {
    core::translation::FormatStringEnumTraits<T>::raw(value);
};
// D std.traits.isIntegral delegates enums to their actual source base type,
// and excludes bool and the character types (traits.d:6252).
template<class T> inline constexpr bool isBrowserIntegral = [] {
    using U = std::remove_cv_t<T>;
    if constexpr (std::is_enum_v<U>) {
        if constexpr (requires { BrowserEnumTraits<U>::sourceIntegral; })
            return BrowserEnumTraits<U>::sourceIntegral;
        else return std::is_integral_v<std::underlying_type_t<U>> &&
            !std::same_as<std::underlying_type_t<U>, bool>;
    } else return std::integral<U> && !std::same_as<U, bool> && !std::same_as<U, char> &&
        !std::same_as<U, wchar_t> && !std::same_as<U, char8_t> &&
        !std::same_as<U, char16_t> && !std::same_as<U, char32_t>;
}();
inline BrowserText text(std::string_view value) {
    return allocateBrowserMemory([&] { return BrowserText(compat::RetainedString(value)); });
}
// Only use for actual static metadata/literal bytes, never a temporary string.
inline BrowserText staticText(std::string_view value) noexcept {
    return compat::RetainedString::fromStaticView(value);
}
inline BrowserText emptyText() noexcept { return compat::RetainedString::emptyLiteral(); }
template<std::size_t N> BrowserText tr(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
inline BrowserText concat(BrowserText a, BrowserText b) {
    return allocateBrowserMemory([&] {
        return BrowserText(compat::RetainedString(std::string(a.view()) + std::string(b.view())));
    });
}
inline BrowserText lower(BrowserText value) {
    return BrowserText(io::depthmap_psd::ngPsdDepthLowerString(value.retainedString()));
}
inline std::string cstr(BrowserText value) {
#ifndef NDEBUG
    auto contents = value.view();
    while (!contents.empty() && contents.back() == '\0') contents.remove_suffix(1);
    if (contents.find('\0') != std::string_view::npos)
        throw compat::AssertError{"toStringz input contains an embedded zero", __FILE__, __LINE__};
#endif
    return std::string(value.view());
}
inline BrowserText join(Texts values, std::string_view separator) {
    return joinBrowserTexts(values, separator);
}
template<class... T> BrowserText format(std::string_view pattern, T&&... args) {
    const std::array<core::translation::TypedFormatArgument,sizeof...(T)> values{
        core::translation::ngFormatArgument(std::forward<T>(args))...};
    auto formatted = core::translation::ngFormatTyped(pattern,values);
    return allocateBrowserMemory([&] {
        return BrowserText(compat::RetainedString(std::move(formatted)));
    });
}
inline bool input(BrowserText id, BrowserText label, BrowserText& value) {
    return widgets::ngInputText(id,label,value);
}
// Phobos std.conv convError and decimal parse, BSL-1.0; original notice:
// native/port/licenses/phobos-unicode.txt. Decode before building the message.
template<std::integral T> [[noreturn]] void integerConversionError(
        std::string_view suffix, std::size_t sourceLine) {
    if (suffix.empty()) {
        // convError's empty-input message is a compile-time concatenation.
        const auto literal = [] {
            if constexpr (sizeof(T) == 1 && std::is_signed_v<T>)
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type byte");
            else if constexpr (sizeof(T) == 1)
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type ubyte");
            else if constexpr (sizeof(T) == 2 && std::is_signed_v<T>)
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type short");
            else if constexpr (sizeof(T) == 2)
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type ushort");
            else if constexpr (sizeof(T) == 4 && std::is_signed_v<T>)
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type int");
            else if constexpr (sizeof(T) == 4)
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type uint");
            else if constexpr (sizeof(T) == 8 && std::is_signed_v<T>)
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type long");
            else {
                static_assert(sizeof(T) == 8);
                return compat::RetainedString::fromStaticLiteral(
                    "Unexpected end of input when converting from type string to type ulong");
            }
        }();
        throw compat::PhobosConvException(literal,
            compat::RetainedString::fromStaticLiteral("std/conv.d"), sourceLine);
    }
    const auto scalarLength = compat::phobos::ngUnexpectedScalarLength(suffix);
    const auto message = allocateBrowserMemory([&] {
        const auto target = core::translation::FormatSourceTypeName<T>::get();
        std::string bytes = "Unexpected '";
        if (suffix.front() == '\n') bytes += "\\n";
        else bytes.append(suffix.data(), scalarLength);
        bytes += "' when converting from type string to type ";
        bytes += target;
        return compat::RetainedString(std::move(bytes));
    });
    throw compat::PhobosConvException(message,
        compat::RetainedString::fromStaticLiteral("std/conv.d"), sourceLine);
}
[[noreturn]] inline void integerConversionOverflow(std::size_t sourceLine) {
    throw compat::PhobosConvOverflowException(
        compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), sourceLine);
}
template<std::integral T> bool parseInteger(std::string_view raw, T& output) {
    try {
        // Phobos first parses sub-int targets as int/uint, then checks narrowing.
        using P = std::conditional_t<(sizeof(T) < 4),
            std::conditional_t<std::is_signed_v<T>, std::int32_t, std::uint32_t>, T>;
        if (raw.empty()) integerConversionError<P>(raw, 2397);
        bool negative = false;
        if constexpr (std::is_signed_v<P>) {
            if (raw.front() == '-' || raw.front() == '+') {
                negative = raw.front() == '-'; raw.remove_prefix(1);
            }
        }
        if (raw.empty() || raw.front() < '0' || raw.front() > '9') integerConversionError<P>(raw, 2397);
        using U = std::make_unsigned_t<P>;
        U value = 0;
        U limit = static_cast<U>((std::numeric_limits<P>::max)());
        if constexpr (std::is_signed_v<P>) if (negative) ++limit;
        std::size_t consumed = 0;
        for (unsigned char c : raw) {
            if (c < '0' || c > '9') break;
            const U digit = static_cast<U>(c - '0');
            if (value > limit / 10 || (value == limit / 10 && digit > limit % 10))
                integerConversionOverflow(2379);
            value = static_cast<U>(value * 10 + digit);
            ++consumed;
        }
        const auto parsed = [&]() -> P {
            if constexpr (std::is_signed_v<P>)
                return std::bit_cast<P>(static_cast<U>(negative ? U(0) - value : value));
            else return value;
        }();
        if constexpr (sizeof(T) < 4) {
            if (parsed < (std::numeric_limits<T>::min)() || parsed > (std::numeric_limits<T>::max)())
                integerConversionOverflow(2305);
        }
        if (consumed != raw.size()) integerConversionError<T>(raw.substr(consumed), 1898);
        output = static_cast<T>(parsed);
        return true;
    } catch (const compat::ThrowableException&) { return false; }
}
inline bool parseBool(BrowserText raw, bool& value) {
    const auto normalized = lower(raw);
    if (normalized.view() == "true" || normalized.view() == "1" || normalized.view() == "yes") {
        value = true; return true;
    }
    if (normalized.view() == "false" || normalized.view() == "0" || normalized.view() == "no") {
        value = false; return true;
    }
    return false;
}
template<std::floating_point T> bool parseFloat(BrowserText raw, T& value) {
    try {
        auto result = phobos_parse::parseStringFloat<T>(raw.view());
        if (std::isnan(result)) return false;
        value = result; return true;
    } catch (const compat::ThrowableException&) { return false; }
}
inline Texts split(BrowserText raw) {
    Texts result;
    std::size_t start = 0;
    for (;;) {
        const auto end = raw.view().find(',',start);
        result.append(raw.slice(start, end == std::string_view::npos ? raw.size() : end));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}
}
