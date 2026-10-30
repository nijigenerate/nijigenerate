#pragma once

#include "fields.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/settings_string.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <common/nullable_string.hpp>
#include <common/shared_array.hpp>
#include <array>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands {

template<class T> concept SourceCommandEnum = requires {
    core::translation::FormatEnumTraits<T>::name;
    core::translation::FormatEnumTraits<T>::members;
};

// Source TW tags carry compile-time declarations; actual fields are stored once
// in the translated Storage, through the existing FieldDescriptor member pointers.
template<class T, CommandLiteral Name, CommandLiteral Description, bool Hidden = false,
    CommandJsonSchema Schema = CommandJsonSchema::unspecified>
struct TW {};

namespace detail {
template<class T> struct UnwrapCommandType { using type = T; };
template<class T, CommandLiteral Name, CommandLiteral Description, bool Hidden, CommandJsonSchema Schema>
struct UnwrapCommandType<TW<T, Name, Description, Hidden, Schema>> { using type = T; };

inline void appendReflectionText(std::string& output, std::string_view text) {
    if (text.empty()) return;
    try {
        output.append(text);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::RetainedString retainReflectionText(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

inline compat::RetainedString toCodeString(std::string_view value) {
    try {
        std::string result = "\"";
        for (const auto byte : value) {
            if (byte == '"') result += '\\';
            result += byte;
        }
        result += '"';
        return compat::RetainedString(std::move(result));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::RetainedString toCodeString(compat::RetainedString value) {
    return toCodeString(value.view());
}
inline compat::RetainedString toCodeString(compat::RetainedStringSlice value) {
    return toCodeString(value.view());
}
inline compat::RetainedString toCodeString(core::settings::SettingsString value) {
    return toCodeString(value.view());
}
inline compat::RetainedString toCodeString(nicxlive::core::common::NullableString value) {
    return toCodeString(value.view());
}
inline compat::RetainedString toCodeString(const std::string& value) {
    return toCodeString(std::string_view(value));
}
template<std::size_t N> compat::RetainedString toCodeString(const char (&value)[N]) {
    return toCodeString(std::string_view(value, N - 1));
}
template<class Char> requires (std::same_as<Char, char8_t> || std::same_as<Char, char16_t> ||
    std::same_as<Char, char32_t> || std::same_as<Char, wchar_t>)
compat::RetainedString toCodeString(std::basic_string_view<Char> value) {
    // The source cast(string) is a code-unit reinterpretation, not UTF conversion.
    return toCodeString(std::string_view(reinterpret_cast<const char*>(value.data()),
        value.size() * sizeof(Char)));
}
template<class Char, class Traits, class Allocator> requires (!std::same_as<Char, char>)
compat::RetainedString toCodeString(const std::basic_string<Char, Traits, Allocator>& value) {
    return toCodeString(std::basic_string_view<Char>(value.data(), value.size()));
}
template<class Char, std::size_t N> requires (!std::same_as<Char, char>)
compat::RetainedString toCodeString(const Char (&value)[N]) {
    return toCodeString(std::basic_string_view<Char>(value, N - 1));
}
template<class Char> concept SourceCommandCharacter =
    !std::is_volatile_v<Char> && (
    std::same_as<std::remove_cv_t<Char>, char> || std::same_as<std::remove_cv_t<Char>, char8_t> ||
    std::same_as<std::remove_cv_t<Char>, char16_t> || std::same_as<std::remove_cv_t<Char>, char32_t> ||
    std::same_as<std::remove_cv_t<Char>, wchar_t>);

template<SourceCommandCharacter Char>
compat::RetainedString toCodeString(compat::Slice<Char> value) {
    // Source isSomeString also accepts mutable and qualified character arrays.
    return toCodeString(value.empty() ? std::string_view{} :
        std::string_view(reinterpret_cast<const char*>(value.data()), value.size() * sizeof(Char)));
}
template<SourceCommandCharacter Char>
compat::RetainedString toCodeString(nicxlive::core::common::SharedArray<Char> value) {
    return toCodeString(value.empty() ? std::string_view{} :
        std::string_view(reinterpret_cast<const char*>(value.data()), value.size() * sizeof(Char)));
}
inline compat::RetainedString toCodeString(std::nullptr_t) {
    return compat::RetainedString::fromStaticLiteral("null");
}
template<class T> requires (!SourceCommandEnum<T> && requires(T value) {
    core::translation::ngFormatArgument(value);
})
compat::RetainedString toCodeString(T value) {
    const std::array arguments{core::translation::ngFormatArgument(value)};
    auto formatted = core::translation::ngFormatTyped("%s", arguments);
    return detail::retainReflectionText(std::move(formatted));
}

// __traits(identifier, arg) names this formal parameter, not its enum value.
// Preserve the source's literal ".arg" rather than repairing generated code.
template<class T> requires SourceCommandEnum<T>
compat::RetainedString toCodeString(T) {
    using Traits = core::translation::FormatEnumTraits<T>;
    const std::string_view name = Traits::name;
    std::string result;
    detail::appendReflectionText(result, name);
    detail::appendReflectionText(result, ".arg");
    return detail::retainReflectionText(std::move(result));
}

template<class T> compat::RetainedString enrichArgDesc(compat::RetainedString baseDesc) {
    if constexpr (!SourceCommandEnum<T>) return baseDesc;
    else {
        using Traits = core::translation::FormatEnumTraits<T>;
        std::string choices;
        detail::appendReflectionText(choices, "(");
        bool first = true;
        for (const auto& member : Traits::members) {
            if (!first) detail::appendReflectionText(choices, "|");
            first = false;
            // Source's ordinary tuple foreach declares the local symbol `mem`.
            detail::appendReflectionText(choices, "mem");
            // String-backed source enums may publish their actual member bytes.
            if constexpr (requires { member.stringValue; }) {
                if (member.stringValue != "mem") {
                    detail::appendReflectionText(choices, "=");
                    detail::appendReflectionText(choices, member.stringValue);
                }
            }
        }
        detail::appendReflectionText(choices, ")");
        if (baseDesc.empty()) return detail::retainReflectionText(std::move(choices));
        std::string result;
        detail::appendReflectionText(result, baseDesc.view());
        detail::appendReflectionText(result, " ");
        detail::appendReflectionText(result, choices);
        return detail::retainReflectionText(std::move(result));
    }
}

} // namespace nijigenerate::commands
