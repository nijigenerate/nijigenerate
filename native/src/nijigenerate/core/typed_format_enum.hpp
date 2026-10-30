#pragma once

#include <string_view>
#include <type_traits>
#include <concepts>

namespace nijigenerate::core::translation {

// Each migrated enum supplies its original source name and declaration-ordered
// members. C++ implementation-specific type strings are not source reflection.
template<class T> struct FormatEnumTraits;
template<class T> struct FormatStringEnumTraits;
template<class T> struct FormatNumericEnumTraits;

// Numeric Source enums represented by nominal value classes declare this
// protocol beside their actual definitions. Conversion alone is not metadata.
template<class T> concept HasNumericEnumFormatMetadata = requires(T value,
        typename FormatNumericEnumTraits<T>::Underlying raw) {
    requires std::integral<typename FormatNumericEnumTraits<T>::Underlying>;
    FormatNumericEnumTraits<T>::name;
    FormatNumericEnumTraits<T>::members;
    { FormatNumericEnumTraits<T>::underlying(value) } ->
        std::same_as<typename FormatNumericEnumTraits<T>::Underlying>;
    { FormatNumericEnumTraits<T>::fromUnderlying(raw) } -> std::same_as<T>;
};

template<class T> struct FormatNumericEnumMember {
    T value;
    std::string_view name;
};
template<class T> requires std::is_enum_v<T>
struct FormatEnumMember {
    T value;
    std::string_view name;
};

} // namespace nijigenerate::core::translation
