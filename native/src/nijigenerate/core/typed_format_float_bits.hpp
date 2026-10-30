#pragma once
// Floating decomposition adapted from Phobos std.math.operations.
// Copyright The D Language Foundation 2000-2011. BSL-1.0; see
// native/port/licenses/phobos-float-format.txt.

#include <array>
#include <bit>
#include <climits>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace nijigenerate::core::translation::detail {

// This producer maps the existing non-Windows x86 SourceReal declaration.
// Other native long-double layouts require their own explicit Source mapping.
#if !defined(_WIN32) && (defined(__i386__) || defined(__x86_64__))
inline constexpr bool formatExtendedRealEnabled = CHAR_BIT == 8
    && std::endian::native == std::endian::little
    && std::numeric_limits<long double>::is_iec559
    && std::numeric_limits<long double>::digits == 64
    && std::numeric_limits<long double>::max_exponent == 16384
    && sizeof(long double) >= 10;
#else
inline constexpr bool formatExtendedRealEnabled = false;
#endif

template<class T> concept FormatExtendedReal =
    formatExtendedRealEnabled && std::same_as<T, long double>;

template<class T> concept FormatFloating =
    std::same_as<T, float> || std::same_as<T, double> || FormatExtendedReal<T>;

template<FormatFloating T> inline constexpr unsigned formatFractionalBits =
    std::same_as<T, float> ? 23 : std::same_as<T, double> ? 52 : 63;
template<FormatFloating T> inline constexpr unsigned formatExponentBits =
    std::same_as<T, float> ? 8 : std::same_as<T, double> ? 11 : 15;
template<FormatFloating T> inline constexpr int formatExponentBias =
    std::same_as<T, float> ? 127 : std::same_as<T, double> ? 1023 : 16383;

struct FloatingFormatBits {
    std::uint64_t fraction;
    unsigned exponent;
    bool negative;
};

template<FormatFloating T> FloatingFormatBits extractFormatFloatingBits(T value) {
    constexpr auto fractionMask = (std::uint64_t{1} << formatFractionalBits<T>) - 1;
    constexpr auto exponentMask = (1u << formatExponentBits<T>) - 1;
    if constexpr (FormatExtendedReal<T>) {
        const auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(value);
        std::uint64_t mantissa = 0;
        for (unsigned i = 0; i < 8; ++i)
            mantissa |= std::uint64_t{bytes[i]} << (i * 8);
        const auto signExponent = static_cast<unsigned>(bytes[8])
            | (static_cast<unsigned>(bytes[9]) << 8);
        // Source masks the stored integer bit and reconstructs it from the
        // exponent for normal values; it does not normalize through arithmetic.
        return {mantissa & fractionMask, signExponent & exponentMask,
            (signExponent & 0x8000u) != 0};
    } else {
        using Bits = std::conditional_t<std::same_as<T, float>, std::uint32_t, std::uint64_t>;
        const auto bits = std::bit_cast<Bits>(value);
        return {bits & fractionMask,
            static_cast<unsigned>((bits >> formatFractionalBits<T>) & exponentMask),
            (bits >> (formatFractionalBits<T> + formatExponentBits<T>)) != 0};
    }
}

} // namespace nijigenerate::core::translation::detail
