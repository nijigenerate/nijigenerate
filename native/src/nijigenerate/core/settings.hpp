#pragma once

#include <boost/json.hpp>
#include "settings_value.hpp"
#include "settings_string_map.hpp"
#include "settings_dialog.hpp"
#include "typed_format_enum.hpp"
#include "typed_format_inspector_enums.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <iterator>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

namespace nijigenerate::core::settings {
std::string incSettingsPath();
compat::RetainedString incSettingsMoveCorruptedFile(compat::RetainedStringSlice path);
// Host-string bridge copies its input and result; source callers use the retained entry.
std::string incSettingsMoveCorruptedFile(const std::string& path);
void incSettingsLoad();
void incSettingsSave();
using SettingsStringArray = compat::Slice<SettingsString>;
using SettingsConversionError = compat::PhobosConvException;
namespace detail {
template<class T, class = void> struct SettingsAuthoredEnum {};
template<class T> struct SettingsAuthoredEnum<T, std::void_t<
    decltype(translation::FormatEnumTraits<T>::name),
    decltype(translation::FormatEnumTraits<T>::members)>> {
    using Metadata = translation::FormatEnumTraits<T>;
    inline static constexpr auto sourceName = Metadata::name;
    inline static constexpr auto values = [] {
        std::array<T, std::size(Metadata::members)> result{};
        for (std::size_t i = 0; i < result.size(); ++i) result[i] = Metadata::members[i].value;
        return result;
    }();
};
}
// Reuse actual authored enum names/members; explicit host registrations remain valid.
template<class T> struct SettingsEnumValues : detail::SettingsAuthoredEnum<T> {};
// C++ enums cannot have floating underlying types. Their ported value wrappers
// supply an underlying type and explicit conversions instead.
template<class T> struct SettingsFloatingEnum {};
// Ported alias-this types expose their source JSON assignment conversion here.
template<class T> struct SettingsInputConversion {};

namespace detail {
// Internal persistence boundary. Assignment replaces the root before validation,
// as the original parseJSON assignment does, including non-object roots.
SettingsJsonValue& settingsStore();
const SettingsJsonValue* findSetting(std::string_view name);
void setSetting(SettingsString name, SettingsJsonValue value);
template<class T> inline constexpr bool settingsEnum = [] {
    if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        if constexpr (!std::is_same_v<Underlying, bool> && !std::is_same_v<Underlying, char> &&
                      !std::is_same_v<Underlying, wchar_t> && !std::is_same_v<Underlying, char8_t> &&
                      !std::is_same_v<Underlying, char16_t> && !std::is_same_v<Underlying, char32_t> &&
                      requires { SettingsEnumValues<T>::values; })
            return SettingsEnumValues<T>::values.size() != 0;
        else return false;
    } else return false;
}();
template<class T> inline constexpr bool floatingEnum = [] {
    if constexpr (requires { typename SettingsFloatingEnum<T>::Underlying; })
        return std::is_floating_point_v<typename SettingsFloatingEnum<T>::Underlying>;
    else return false;
}();
// A ported string enum supplies its actual immutable Source value through
// its authored declaration provider, independently of native numeric enums.
template<class T> inline constexpr bool settingsStringEnum = requires(const T& value) {
    { translation::FormatStringEnumTraits<T>::raw(value) } -> std::same_as<compat::RetainedString>;
};
template<class T> inline constexpr bool scalarSetting =
    (std::is_arithmetic_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, wchar_t> &&
        !std::is_same_v<T, char8_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>) ||
    std::is_same_v<T, std::string> || std::is_same_v<T, SettingsString> || settingsEnum<T> || floatingEnum<T>;
template<class T> inline constexpr bool directSetting = scalarSetting<T> ||
    std::is_same_v<T, SettingsJsonArray> || std::is_same_v<T, SettingsJsonObject>;

template<class T> struct InputSlice { static constexpr bool value = false; };
template<class T> struct InputSlice<compat::Slice<T>> {
    static constexpr bool value = true;
    static constexpr bool fixed = false;
    using Element = T;
};
template<class T> struct InputSlice<std::vector<T>> {
    static constexpr bool value = true;
    static constexpr bool fixed = false;
    using Element = T;
};
template<class T, std::size_t N> struct InputSlice<std::array<T, N>> {
    static constexpr bool value = true;
    static constexpr bool fixed = true;
    using Element = T;
};
template<class T> struct InputMap { static constexpr bool value = false; };
template<> struct InputMap<SettingsStringMap> {
    static constexpr bool value = true;
    using Value = SettingsString;
};
template<class V, class H, class E, class A> struct InputMap<std::unordered_map<std::string, V, H, E, A>> {
    static constexpr bool value = true;
    using Value = V;
};
template<class T> inline constexpr bool settingInput = [] {
    if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T> || floatingEnum<T> ||
        std::is_same_v<T, SettingsString> || std::is_same_v<T, std::string> ||
        std::is_same_v<T, SettingsJsonValue> || std::is_same_v<T, SettingsJsonObject> ||
        std::is_same_v<T, SettingsJsonArray> || std::is_same_v<T, boost::json::value> ||
        std::is_same_v<T, std::nullptr_t>) return true;
    else if constexpr (requires(const T& input) { SettingsInputConversion<T>::convert(input); })
        return settingInput<std::remove_cvref_t<decltype(SettingsInputConversion<T>::convert(std::declval<const T&>()))>>;
    else if constexpr (settingsStringEnum<T>) return true;
    else if constexpr (InputSlice<T>::value) {
        if constexpr (std::is_same_v<typename InputSlice<T>::Element, SettingsJsonMember>) return !InputSlice<T>::fixed;
        else return settingInput<typename InputSlice<T>::Element>;
    }
    else if constexpr (InputMap<T>::value) return settingInput<typename InputMap<T>::Value>;
    else return false;
}();

inline void appendSettingCodepoint(std::string& out, std::uint32_t value) {
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) value = 0xfffd;
    if (value < 0x80) out += static_cast<char>(value);
    else if (value < 0x800) {
        out += static_cast<char>(0xc0 | (value >> 6));
        out += static_cast<char>(0x80 | (value & 0x3f));
    } else if (value < 0x10000) {
        out += static_cast<char>(0xe0 | (value >> 12));
        out += static_cast<char>(0x80 | ((value >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (value & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (value >> 18));
        out += static_cast<char>(0x80 | ((value >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((value >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (value & 0x3f));
    }
}

template<class T> SettingsJsonValue settingValue(const T& input) {
    if constexpr (requires { SettingsInputConversion<T>::convert(input); })
        return settingValue(SettingsInputConversion<T>::convert(input));
    else if constexpr (settingsStringEnum<T>)
        return SettingsJsonValue(SettingsString(translation::FormatStringEnumTraits<T>::raw(input)));
    else if constexpr (std::is_enum_v<T>)
        return settingValue(static_cast<std::underlying_type_t<T>>(input));
    else if constexpr (floatingEnum<T>) return settingValue(SettingsFloatingEnum<T>::toNumber(input));
    else if constexpr (std::is_same_v<T, char> || std::is_same_v<T, char8_t>)
        return SettingsJsonValue(static_cast<std::int64_t>(static_cast<unsigned char>(input)));
    else if constexpr (std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t> || std::is_same_v<T, wchar_t>)
        return SettingsJsonValue(static_cast<std::int64_t>(input));
    else if constexpr (InputSlice<T>::value && !std::is_same_v<T, SettingsJsonArray>) {
        using Element = typename InputSlice<T>::Element;
        if constexpr (std::is_same_v<Element, SettingsJsonMember>) {
            SettingsJsonObject object(true);
            if constexpr (std::is_same_v<T, compat::Slice<SettingsJsonMember>>) object.orderedMembers() = input;
            else object.orderedMembers() = compat::Slice<SettingsJsonMember>(input.begin(), input.end());
            return SettingsJsonValue(std::move(object));
        } else if constexpr (!InputSlice<T>::fixed && (std::is_same_v<Element, char> || std::is_same_v<Element, char8_t>)) {
            try {
                std::string bytes;
                for (const auto c : input) bytes += static_cast<char>(c);
                return bytes.empty() ? SettingsJsonValue(SettingsString{}) : SettingsJsonValue(std::move(bytes));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        } else if constexpr (!InputSlice<T>::fixed &&
            (std::is_same_v<Element, char16_t> || std::is_same_v<Element, char32_t>)) {
            try {
                std::string bytes;
                for (std::size_t i = 0; i < input.size(); ++i) {
                    std::uint32_t scalar = input[i];
                    if constexpr (std::is_same_v<Element, char16_t>) {
                        if (scalar >= 0xd800 && scalar <= 0xdbff) {
                            if (i + 1 == input.size())
                                scalar = 0xfffd;
                            else {
                                const auto next = input[++i];
                                scalar = next < 0xdc00 || next > 0xdfff ? 0xfffd :
                                    0x10000 + ((scalar - 0xd800) << 10) + (next - 0xdc00);
                            }
                        }
                    }
                    appendSettingCodepoint(bytes, scalar);
                }
                return bytes.empty() ? SettingsJsonValue(SettingsString{}) : SettingsJsonValue(std::move(bytes));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        } else {
            // Phobos allocates every destination cell before converting elements.
            const auto count = input.size();
            auto values = [&] {
                try {
                    return SettingsJsonArray{std::vector<SettingsJsonValue>(count)};
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            for (std::size_t i = 0; i < input.size(); ++i) {
                // Source foreach captures the element value before conversion.
                Element item = input[i];
                values[i] = settingValue(item);
            }
            return SettingsJsonValue(std::move(values));
        }
    } else if constexpr (InputMap<T>::value) {
        SettingsJsonObject values;
        for (const auto& [key, item] : input) {
            // Source foreach retains both values before running a conversion.
            const auto retainedKey = key;
            const typename InputMap<T>::Value retainedItem = item;
            values[retainedKey] = settingValue(retainedItem);
        }
        return SettingsJsonValue(std::move(values));
    } else return SettingsJsonValue(input);
}

// Resolve the actual Source assignment branch without evaluating a conversion.
template<class T> consteval auto settingInputResult() {
    if constexpr (requires(const T& input) { SettingsInputConversion<T>::convert(input); })
        return settingInputResult<std::remove_cvref_t<decltype(
            SettingsInputConversion<T>::convert(std::declval<const T&>()))>>();
    else if constexpr (settingsStringEnum<T>)
        return std::type_identity<SettingsString>{};
    else if constexpr (std::is_enum_v<T>)
        return settingInputResult<std::underlying_type_t<T>>();
    else if constexpr (floatingEnum<T>)
        return settingInputResult<std::remove_cvref_t<decltype(
            SettingsFloatingEnum<T>::toNumber(std::declval<const T&>()))>>();
    else return std::type_identity<T>{};
}

template<class T> void assignSettingValue(SettingsJsonValue& target, const T& input) {
    using Result = typename decltype(settingInputResult<T>())::type;
    if constexpr (std::is_same_v<Result, SettingsJsonValue> || std::is_same_v<Result, boost::json::value>) {
        // Copy both the actual type and Store, including a bool/null's hidden Store.
        target = settingValue(input);
    } else if constexpr (std::is_same_v<Result, bool>) {
        // Source evaluates the boolean before assigning true_/false_; no Store write.
        target = settingValue(input).as_bool();
    } else if constexpr (std::is_same_v<Result, std::nullptr_t>) {
        // Source's null branch does not evaluate or store a payload.
        target = nullptr;
    } else {
        constexpr auto type = [] {
            if constexpr (std::is_same_v<Result, SettingsString> || std::is_same_v<Result, std::string>)
                return SettingsJsonType::String;
            else if constexpr (std::is_same_v<Result, SettingsJsonObject> || InputMap<Result>::value)
                return SettingsJsonType::Object;
            else if constexpr (InputSlice<Result>::value) {
                using Element = typename InputSlice<Result>::Element;
                if constexpr (std::is_same_v<Element, SettingsJsonMember>) return SettingsJsonType::Object;
                else if constexpr (!InputSlice<Result>::fixed &&
                    (std::is_same_v<Element, char> || std::is_same_v<Element, char8_t> ||
                     std::is_same_v<Element, char16_t> || std::is_same_v<Element, char32_t>))
                    return SettingsJsonType::String;
                else return SettingsJsonType::Array;
            } else if constexpr (std::is_floating_point_v<Result>) return SettingsJsonType::Floating;
            else if constexpr (std::is_integral_v<Result>) {
                if constexpr (std::is_same_v<Result, char> || std::is_same_v<Result, char8_t> ||
                    std::is_same_v<Result, char16_t> || std::is_same_v<Result, char32_t> ||
                    std::is_same_v<Result, wchar_t>) return SettingsJsonType::Integer;
                else if constexpr (std::is_unsigned_v<Result>) return SettingsJsonType::UInteger;
                else return SettingsJsonType::Integer;
            } else {
                static_assert(std::is_same_v<Result, void>, "Unsupported settings JSON assignment input");
            }
        }();
        target.template sourceAssignConverted<type>([&] { return settingValue(input); });
    }
}
template<class T, class S> T settingIntegral(S number) {
    if (!std::in_range<T>(number)) {
        bool negative = false;
        if constexpr (std::is_signed_v<S>) negative = number < 0;
        const auto line = sizeof(T) == sizeof(S)
            ? (negative ? std::size_t{568} : std::size_t{562})
            : (negative ? std::size_t{1581} : std::size_t{1587});
        const auto message = negative
            ? compat::RetainedString::fromStaticLiteral("Conversion negative overflow")
            : compat::RetainedString::fromStaticLiteral("Conversion positive overflow");
        throw compat::PhobosConvOverflowException(message,
            compat::RetainedString::fromStaticLiteral("std/conv.d"), line);
    }
    return static_cast<T>(number);
}
template<class T> T getScalar(const SettingsJsonValue& value) {
    static_assert(directSetting<T>);
    if constexpr (std::is_same_v<T, SettingsString>) {
        return value.as_string();
    } else if constexpr (std::is_same_v<T, SettingsJsonArray>) {
        return value.sourceArrayNoRef();
    } else if constexpr (std::is_same_v<T, SettingsJsonObject>) {
        return value.sourceObjectRef();
    } else if constexpr (settingsEnum<T>) {
        if (!value.is_int64() && !value.is_uint64())
            throw SettingsJsonError::fromStaticLiteral("JSONValue is not a an integral type");
        for (const auto member : SettingsEnumValues<T>::values)
            if (SettingsJsonValue(static_cast<std::underlying_type_t<T>>(member)) == value) return member;
        // Source T.stringof is authored metadata, never a native RTTI spelling.
        if constexpr (requires { SettingsEnumValues<T>::sourceName; }) {
            const bool isUnsigned = value.is_uint64();
            const auto number = isUnsigned ? value.as_uint64()
                : std::bit_cast<std::uint64_t>(value.as_int64());
            const std::string_view sourceName = SettingsEnumValues<T>::sourceName;
            compat::RetainedString message;
            try {
                std::string text("Value (");
                text += isUnsigned ? std::to_string(number)
                    : std::to_string(std::bit_cast<std::int64_t>(number));
                text += ") does not match any member value of enum '";
                text += sourceName;
                text += '\'';
                message = compat::RetainedString(std::move(text));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
            throw compat::PhobosConvException(message,
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2133);
        } else {
            // Legacy host registrations lack the source enum name. Keep their
            // accepted domain while exposing the genuine conversion family.
            throw compat::PhobosConvException(compat::RetainedString::fromStaticLiteral(
                "Value does not match any declared enum member"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2133);
        }
    } else if constexpr (floatingEnum<T>) {
        using Underlying = typename SettingsFloatingEnum<T>::Underlying;
        return SettingsFloatingEnum<T>::fromNumber(getScalar<Underlying>(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
        const auto text = value.as_string().view();
        try {
            return text.empty() ? std::string{} : std::string(text);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        return value.as_bool();
    } else if constexpr (std::is_floating_point_v<T>) {
        if (value.is_double()) return static_cast<T>(value.as_double());
        if (value.is_uint64()) return static_cast<T>(value.as_uint64());
        if (value.is_int64()) return static_cast<T>(value.as_int64());
        throw SettingsJsonError::fromStaticLiteral("JSONValue is not a number type");
    } else {
        if (value.is_uint64()) {
            return settingIntegral<T>(value.as_uint64());
        }
        if (value.is_int64()) {
            return settingIntegral<T>(value.as_int64());
        }
        throw SettingsJsonError::fromStaticLiteral("JSONValue is not a an integral type");
    }
}
template<class T> T initialSetting() {
    if constexpr (std::is_floating_point_v<T>) return std::numeric_limits<T>::quiet_NaN();
    else if constexpr (settingsEnum<T>) return SettingsEnumValues<T>::values[0];
    else if constexpr (floatingEnum<T>) return SettingsFloatingEnum<T>::initialValue();
    else return {};
}
}

template<class T> requires(detail::directSetting<T> || std::is_same_v<T, SettingsStringArray> ||
    std::is_same_v<T, SettingsStringMap>)
T incSettingsGet(SettingsString name) {
    auto* value = detail::findSetting(name.view());
    if (!value) return detail::initialSetting<T>();
    if constexpr (std::is_same_v<T, SettingsStringArray>) {
        T result;
        const auto array = value->sourceArrayValue();
        for (auto item : array) result.append(detail::getScalar<SettingsString>(item));
        return result;
    } else if constexpr (std::is_same_v<T, SettingsStringMap>) {
        const auto object = value->sourceObjectRef();
        T result;
        for (const auto& entry : object) {
            const auto key = entry.name;
            const auto item = entry.value();
            result[key] = detail::getScalar<SettingsString>(item);
        }
        return result;
    } else return detail::getScalar<T>(*value);
}
template<class T> requires(detail::directSetting<T>)
T incSettingsGet(SettingsString name, T defaultValue) {
    if (auto* value = detail::findSetting(name.view())) return detail::getScalar<T>(*value);
    return defaultValue;
}
template<class T> requires(detail::settingInput<T> || std::is_same_v<T, SettingsStringArray> ||
    std::is_same_v<T, SettingsStringMap>)
void incSettingsSet(SettingsString name, T value) {
    if constexpr (std::is_same_v<T, SettingsStringArray>) {
        SettingsJsonArray array;
        for (const auto& item : value) array.append(SettingsJsonValue(item));
        detail::setSetting(name, std::move(array));
    } else if constexpr (std::is_same_v<T, SettingsStringMap>) {
        SettingsJsonObject object;
        for (const auto& [key, item] : value) object[key] = SettingsJsonValue(item);
        detail::setSetting(name, std::move(object));
    } else {
        detail::settingsStore().sourceIndexAssign(std::move(name),
            [&](SettingsJsonValue& target) { detail::assignSettingValue(target, value); },
            [&] { return detail::settingValue(value); });
    }
}
bool incSettingsCanGet(SettingsString name);

} // namespace nijigenerate::core::settings
