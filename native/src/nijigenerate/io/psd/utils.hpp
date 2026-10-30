#pragma once
// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace nijigenerate::io::psd::utils {

namespace detail {
template<class T, bool = std::is_enum_v<T>> struct Base { using type = T; };
template<class T> struct Base<T, true> { using type = std::underlying_type_t<T>; };
template<class T> using BaseType = typename Base<T>::type;
}

template<class T> concept Integral = std::integral<detail::BaseType<T>> &&
    !std::same_as<detail::BaseType<T>, bool> && !std::same_as<detail::BaseType<T>, char> &&
    !std::same_as<detail::BaseType<T>, wchar_t> && !std::same_as<detail::BaseType<T>, char8_t> &&
    !std::same_as<detail::BaseType<T>, char16_t> && !std::same_as<detail::BaseType<T>, char32_t>;

namespace detail {
template<Integral T> constexpr T fromBits(std::make_unsigned_t<detail::BaseType<T>> value) noexcept {
    using B = BaseType<T>;
    if constexpr (std::is_signed_v<B>) return static_cast<T>(std::bit_cast<B>(value));
    else return static_cast<T>(value);
}
}

template<Integral T> constexpr bool isPowerOfTwo(T x) noexcept {
    // D promotes small integers to int and otherwise wraps at the original width.
    using U = std::conditional_t<(sizeof(T) < 4), std::uint32_t, std::make_unsigned_t<detail::BaseType<T>>>;
    const U bits = static_cast<U>(x);
    return (bits & (bits - U{1})) == 0;
}

template<Integral T> constexpr T roundUpToMultiple(T value, T multiple) noexcept {
    assert(isPowerOfTwo(multiple) && "Expected power-of-two multiplier");
    using U = std::conditional_t<(sizeof(T) < 4), std::uint32_t, std::make_unsigned_t<detail::BaseType<T>>>;
    const U mask = static_cast<U>(multiple) - U{1};
    return detail::fromBits<T>(static_cast<std::make_unsigned_t<detail::BaseType<T>>>(
        (static_cast<U>(value) + mask) & ~mask));
}

template<Integral T> constexpr T roundDownToMultiple(T value, T multiple) noexcept {
    assert(isPowerOfTwo(multiple) && "Expected power-of-two multiplier");
    using U = std::conditional_t<(sizeof(T) < 4), std::uint32_t, std::make_unsigned_t<detail::BaseType<T>>>;
    const U mask = static_cast<U>(multiple) - U{1};
    return detail::fromBits<T>(static_cast<std::make_unsigned_t<detail::BaseType<T>>>(static_cast<U>(value) & ~mask));
}
} // namespace nijigenerate::io::psd::utils
