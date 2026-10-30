#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/node_class_name.hpp>
#include <nijigenerate/windows/command_browser_value_traits.hpp>
#include <nijigenerate/windows/command_browser_enum_traits.hpp>
#include <iterator>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::api::mcp::helpers::detail {
inline compat::RetainedString retainHelperValueString(std::string bytes) {
    try { return compat::RetainedString(std::move(bytes)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline void appendHelperValueBytes(std::string& output, std::string_view bytes) {
    try { output.append(bytes); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> constexpr bool sourceText =
    std::same_as<T,compat::RetainedString> || std::same_as<T,compat::RetainedStringSlice> ||
    std::same_as<T,core::settings::SettingsString> || std::same_as<T,std::string> ||
    std::same_as<T,std::string_view> || std::same_as<T,nicxlive::core::common::NullableString>;
template<class T> constexpr bool sourceCharacter =
    std::same_as<T,char> || std::same_as<T,char8_t> || std::same_as<T,char16_t> ||
    std::same_as<T,char32_t> || std::same_as<T,wchar_t>;

template<class T> compat::RetainedString retainedText(T value) {
    if constexpr(std::same_as<T,compat::RetainedStringSlice> ||
                 std::same_as<T,core::settings::SettingsString>) return value.retainedString();
    else return compat::RetainedString(std::move(value));
}

template<class T> inline constexpr bool sourceObjectReference = [] {
    if constexpr (requires {
        typename T::element_type;
        nicxlive::core::d_runtime::SourceTypeSpellings<typename T::element_type>::isNativeObject;
    })
        return nicxlive::core::d_runtime::SourceTypeSpellings<typename T::element_type>::isNativeObject;
    else return false;
}();

template<class T> compat::RetainedString sourceClassFormatImpl(T value) {
    if (!value) return compat::RetainedString::fromStaticLiteral("null");
    if constexpr (std::derived_from<typename T::element_type,nicxlive::core::param::Parameter>)
        compat::ngRegisterResourceClassNames();
    if constexpr (requires { value->toString(); }) return retainedText(value->toString());
    else return windows::command_browser::detail::browserValueString(value).retainedString();
}

template<class T> compat::RetainedString sourceDeclaredValueToStr(T value) {
    if constexpr (sourceObjectReference<T>) return sourceClassFormatImpl<T>(value);
    else {
        std::string bytes;
        core::translation::TypedFormatWriter writer(bytes);
        const core::translation::FormatSpec spec;
        core::translation::ngFormatValueTo(writer, value, spec);
        return retainHelperValueString(std::move(bytes));
    }
}

template<class T> compat::RetainedString sourceDeclaredValueToImpl(T value) {
    return sourceDeclaredValueToStr<T>(value);
}

template<class T> compat::RetainedString sourceDeclaredValueTo(T value) {
    return sourceDeclaredValueToImpl<T>(value);
}

template<class T, std::size_t N>
compat::RetainedString sourceFixedArrayToStr(std::array<T, N> value) {
    const std::array arguments{core::translation::ngFormatFixedArrayReferenceArgument(value)};
    return retainHelperValueString(core::translation::ngFormatTyped("%s", arguments));
}

template<class T, std::size_t N>
compat::RetainedString sourceFixedArrayToImpl(std::array<T, N> value) {
    return sourceFixedArrayToStr(value);
}

template<class T, std::size_t N>
compat::RetainedString sourceValueString(const std::array<T, N>& value) {
    // std.conv's static-array to overload is ref; toImpl and toStr own values.
    return sourceFixedArrayToImpl(value);
}

// Use actual canonical element producers. Other native/class range profiles
// retain their existing real toString path rather than probing a formatter body.
template<class T> requires (sourceText<T> || sourceCharacter<T> || std::is_arithmetic_v<T> ||
    compat::SourceFixedArray<std::remove_cv_t<T>>::value ||
    core::translation::HasNumericEnumFormatMetadata<T> ||
    (std::is_enum_v<T> && requires { core::translation::FormatEnumTraits<T>::members; }) ||
    core::translation::HasAggregateFormatMetadata<std::remove_cv_t<T>> ||
    core::translation::HasFormatObjectTraits<T> || core::translation::HasCustomFormat<T>)
compat::RetainedString sourceValueString(const compat::Slice<T>& value) {
    const std::array arguments{core::translation::ngFormatArgument(value)};
    return retainHelperValueString(core::translation::ngFormatTyped("%s",arguments));
}

template<class T> compat::RetainedString sourceValueString(const T& value) {
    if constexpr (core::translation::HasNumericEnumFormatMetadata<T>) {
        const std::array arguments{core::translation::ngFormatArgument(value)};
        return retainHelperValueString(core::translation::ngFormatTyped("%s",arguments));
    } else if constexpr (std::is_enum_v<T> && !windows::command_browser::detail::isBrowserEnum<T> &&
            requires { core::translation::FormatEnumTraits<T>::members; }) {
        const std::array arguments{core::translation::ngFormatArgument(value)};
        return retainHelperValueString(core::translation::ngFormatTyped("%s",arguments));
    } else if constexpr(std::is_enum_v<T>) {
        // std.conv enumRep delegates %s to the first declared equal member name,
        // including string-base enums; it does not expose the underlying spelling.
        return windows::command_browser::detail::browserValueString(value).retainedString();
    } else if constexpr(requires { typename T::element_type; }) {
        if constexpr (sourceObjectReference<T>) return sourceDeclaredValueTo<T>(value);
        else {
            if(!value) return compat::RetainedString::fromStaticLiteral("null");
            if constexpr(std::derived_from<typename T::element_type,nicxlive::core::param::Parameter>)
                compat::ngRegisterResourceClassNames();
            if constexpr(requires { value->toString(); }) return retainedText(value->toString());
            else return windows::command_browser::detail::browserValueString(value).retainedString();
        }
    } else if constexpr(sourceText<T> || std::is_arithmetic_v<T>) {
        const std::array arguments{core::translation::ngFormatArgument(value)};
        return retainHelperValueString(core::translation::ngFormatTyped("%s",arguments));
    } else if constexpr (core::translation::HasCustomFormat<T> ||
            core::translation::HasAggregateFormatMetadata<std::remove_cv_t<T>>) {
        return sourceDeclaredValueTo<T>(value);
    } else if constexpr(requires { std::begin(value); std::end(value); }) {
        using Element=std::remove_cvref_t<decltype(*std::begin(value))>;
        if constexpr(sourceCharacter<Element>) {
            const compat::Slice<Element> characters(std::begin(value),std::end(value));
            const std::array arguments{core::translation::ngFormatArgument(characters)};
            return retainHelperValueString(core::translation::ngFormatTyped("%s",arguments));
        } else {
            std::string output;
            appendHelperValueBytes(output,"[");
            bool first=true;
            for(const auto& entry:value) {
                if(!first) appendHelperValueBytes(output,", ");
                first=false;
                if constexpr(sourceText<Element> || sourceCharacter<Element>) {
                    core::translation::detail::ParsedTypedFormat spec;
                    spec.conversion='s';
                    appendHelperValueBytes(output,core::translation::detail::formatTypedElement(
                        core::translation::ngFormatArgument(entry),spec));
                } else appendHelperValueBytes(output,sourceValueString(entry).view());
            }
            appendHelperValueBytes(output,"]");
            return retainHelperValueString(std::move(output));
        }
    } else if constexpr(requires { value.toString(); }) return retainedText(value.toString());
    else return windows::command_browser::detail::browserValueString(value).retainedString();
}
} // namespace nijigenerate::api::mcp::helpers::detail
