#pragma once

// Decimal-token subset of current Phobos std.conv.parse used by std.json.
// BSL-1.0 attribution: native/port/licenses/phobos-rig-json.txt.
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <cmath>
#include <cfloat>
#include <string>
#include <bit>
#include <cstdint>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::autorig::json::detail {

using ArtifactConvError = ::nijigenerate::compat::PhobosConvException;
using ArtifactConvOverflowError = ::nijigenerate::compat::PhobosConvOverflowException;

template<class Operation> decltype(auto) allocateArtifactNumberText(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline std::uint64_t firstPassInteger(std::string_view token, bool negative) {
    const std::uint64_t limit = negative ? std::uint64_t{1} << 63 : std::numeric_limits<std::uint64_t>::max();
    std::uint64_t result = 0;
    for (std::size_t i = negative ? 1 : 0; i < token.size(); ++i) {
        const unsigned digit = static_cast<unsigned>(token[i] - '0');
        if (result > (limit - digit) / 10)
            throw ArtifactConvOverflowError(
                compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2527);
        result = result * 10 + digit;
    }
    return result;
}

namespace numeric {
#if defined(_MSC_VER)
using Real = double;
#else
using Real = long double;
#endif
// LDC's source real matches the target C long-double ABI (64 bits for MSVC).
#if !defined(_MSC_VER) && LDBL_MAX_EXP > 1024
inline constexpr std::array<Real, 13> positive{
    1e4096L,1e2048L,1e1024L,1e512L,1e256L,1e128L,1e64L,
    1e32L,1e16L,1e8L,1e4L,1e2L,1e1L};
inline constexpr std::array<Real, 14> negative{
    1e-4096L,1e-2048L,1e-1024L,1e-512L,1e-256L,1e-128L,1e-64L,
    1e-32L,1e-16L,1e-8L,1e-4L,1e-2L,1e-1L,1.0L};
#else
inline constexpr std::array<Real, 13> positive{
    std::numeric_limits<Real>::infinity(), std::numeric_limits<Real>::infinity(),
    std::numeric_limits<Real>::infinity(), std::numeric_limits<Real>::infinity(),
    1e256,1e128,1e64,1e32,1e16,1e8,1e4,1e2,1e1};
inline constexpr std::array<Real, 14> negative{
    0,0,0,0,1e-256,1e-128,1e-64,1e-32,1e-16,1e-8,1e-4,1e-2,1e-1,1};
#endif
inline unsigned char lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}
inline bool digit(unsigned char c) { return c >= '0' && c <= '9'; }
inline bool hexDigit(unsigned char c) { return digit(c) || (lower(c) >= 'a' && lower(c) <= 'f'); }

// Current LDC core.math: Digital Mars 2000-2011, Walter Bright/Don Clugston.
// BSL-1.0 in phobos-rig-json.txt. The x87 branch uses actual fscale; the
// other source formats use LDC's exponent-word lowering, not libm errno rules.
template<class R> inline R sourceLdexp(R value, int exponent) {
    if constexpr (std::numeric_limits<R>::digits == 64) {
#if !defined(_MSC_VER) && (defined(__i386__) || defined(__x86_64__))
        R result;
        __asm__ __volatile__("fildl %1; fxch %%st(1); fscale; fstp %%st(1)"
            : "=t"(result) : "m"(exponent), "0"(value) : "st(1)", "cc");
        return result;
#else
        static_assert(std::numeric_limits<R>::digits != 64, "Source real80 requires the actual x87 target");
#endif
    } else {
        static_assert(std::numeric_limits<R>::digits == 53 || std::numeric_limits<R>::digits == 113);
        using U = std::conditional_t<(std::numeric_limits<R>::digits == 113 || sizeof(R) == sizeof(std::size_t)),
            std::uint64_t, std::uint32_t>;
        constexpr unsigned wordBits = sizeof(U) * 8;
        constexpr std::size_t count = sizeof(R) / sizeof(U);
        constexpr std::size_t low = std::endian::native == std::endian::little ? 0 : 1;
        constexpr std::size_t high = count > 1 ? 1 - low : 0;
        constexpr unsigned shift = std::numeric_limits<R>::digits - 1 - (count > 1 ? wordBits : 0);
        constexpr U signMask = U(1) << (wordBits - 1);
        constexpr U mantissaMask = (U(1) << shift) - 1;
        constexpr U exponentMask = (std::numeric_limits<U>::max() >> (shift + 1)) << shift;
        constexpr int maximumExponent = static_cast<int>(exponentMask >> shift);
        auto words = std::bit_cast<std::array<U, count>>(value);
        U upper = words[high];
        int storedExponent = static_cast<int>((upper & exponentMask) >> shift);
        if (storedExponent == maximumExponent) return value;
        if (storedExponent == 0) {
            constexpr R factor = R(1) / std::numeric_limits<R>::epsilon();
            const volatile R normalized = value * factor;
            value = normalized;
            words = std::bit_cast<std::array<U, count>>(value);
            upper = words[high];
            storedExponent = static_cast<int>((upper & exponentMask) >> shift) -
                (std::numeric_limits<R>::digits - 1);
        }
        const auto sum = static_cast<std::int64_t>(exponent) + storedExponent;
        const bool overflow = sum < std::numeric_limits<std::int32_t>::min() ||
            sum > std::numeric_limits<std::int32_t>::max();
        exponent = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(sum));
        if (overflow || exponent >= maximumExponent) {
            upper = (upper & signMask) ^ exponentMask;
            if constexpr (count > 1) words[low] = 0;
        } else if (exponent > 0) {
            upper = (upper & ~exponentMask) ^ (static_cast<U>(exponent) << shift);
        } else {
            exponent = std::bit_cast<std::int32_t>(std::uint32_t(1) - static_cast<std::uint32_t>(exponent));
            U mantissa = upper & mantissaMask;
            if (exponent > std::numeric_limits<R>::digits) {
                exponent = std::numeric_limits<R>::digits;
                if constexpr (count > 1) words[low] = 0;
            }
            upper &= signMask;
            mantissa ^= mantissaMask + 1;
            const auto right = [](U n, int bits) { return n >> (static_cast<unsigned>(bits) & (wordBits - 1)); };
            const auto left = [](U n, int bits) { return n << (static_cast<unsigned>(bits) & (wordBits - 1)); };
            if constexpr (count > 1) {
                const int remaining = std::bit_cast<std::int32_t>(
                    static_cast<std::uint32_t>(exponent) - wordBits);
                if (remaining < 0) {
                    words[low] = right(words[low], exponent) ^ left(mantissa,
                        std::bit_cast<std::int32_t>(wordBits - static_cast<std::uint32_t>(exponent)));
                    upper ^= right(mantissa, exponent);
                } else words[low] = remaining < static_cast<int>(wordBits) ? right(mantissa, remaining) : 0;
            } else upper ^= right(mantissa, exponent);
        }
        words[high] = upper;
        return std::bit_cast<R>(words);
    }
}
}

// Same Phobos provider for JSON's parse!double and non-Windows parse!real.
// Prefix consumption, special values and malformed-token failures matter for
// duplicate-key tag mismatches in the restoring walk; no strtold substitute.
template<class Target> inline Target parseSourceFloating(std::string_view source) {
    std::size_t cursor = 0;
    const auto empty = [&] { return cursor == source.size(); };
    const auto front = [&] { return static_cast<unsigned char>(source[cursor]); };
    const auto fail = [&](std::string_view message, std::size_t line) -> void {
        throw ArtifactConvError(allocateArtifactNumberText([&] {
            return compat::RetainedString(std::string(message));
        }),
            compat::RetainedString::fromStaticLiteral("std/conv.d"), line);
    };
    const auto bail = [&](std::string_view message, std::size_t line) -> void {
        const auto text = allocateArtifactNumberText([&] {
            std::string result(message);
            result += " for input \"";
            result.append(source);
            result += "\".";
            return result;
        });
        fail(text, line);
    };
    if (empty()) bail("Floating point conversion error", 3164);
    bool negative = false;
    bool infinityPrefix = false;
    switch (front()) {
    case '-':
        negative = true;
        ++cursor;
        if (empty()) bail("Floating point conversion error", 3175);
        infinityPrefix = numeric::lower(front()) == 'i';
        break;
    case '+':
        ++cursor;
        if (empty()) bail("Floating point conversion error", 3182);
        break;
    case 'i': case 'I':
        infinityPrefix = true;
        break;
    default: break;
    }
    if (infinityPrefix) {
        ++cursor;
        if (empty() || numeric::lower(front()) != 'n')
            bail("error converting input to floating point", 3189);
        ++cursor;
        if (empty() || numeric::lower(front()) != 'f')
            bail("error converting input to floating point", 3193);
        ++cursor;
        return negative ? -std::numeric_limits<Target>::infinity() : std::numeric_limits<Target>::infinity();
    }
    bool hex = false;
    const bool startsWithZero = front() == '0';
    if (startsWithZero) {
        ++cursor;
        if (empty()) return negative ? -Target(0) : Target(0);
        hex = front() == 'x' || front() == 'X';
        if (hex) ++cursor;
    } else if (numeric::lower(front()) == 'n') {
        ++cursor;
        if (empty() || numeric::lower(front()) != 'a')
            bail("error converting input to floating point", 3241);
        ++cursor;
        if (empty() || numeric::lower(front()) != 'n')
            bail("error converting input to floating point", 3245);
        ++cursor;
        return std::numeric_limits<Target>::quiet_NaN();
    }
    std::uint64_t msdec = 0, lsdec = 0, scale = 1;
    std::int32_t exponent = 0;
    unsigned dot = 0;
    bool sawDigits = hex ? false : startsWithZero;
    const unsigned base = hex ? 16 : 10;
    const unsigned step = hex ? 4 : 1;
    const std::uint64_t scaleMaximum = hex ? 0x1000'0000'0000'0000ULL : 10'000'000'000'000'000'000ULL;
    const auto addExponent = [&](std::int32_t amount) {
        exponent = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(exponent) +
            static_cast<std::uint32_t>(amount));
    };
    const auto isDigit = [&](unsigned char c) { return hex ? numeric::hexDigit(c) : numeric::digit(c); };
    while (!empty()) {
        int c = front();
        while (isDigit(static_cast<unsigned char>(c))) {
            sawDigits = true;
            const unsigned digit = numeric::digit(static_cast<unsigned char>(c)) ?
                c - '0' : numeric::lower(static_cast<unsigned char>(c)) - 'a' + 10;
            if (msdec < (std::numeric_limits<std::uint64_t>::max() - base) / base)
                msdec = msdec * base + digit;
            else if (scale < scaleMaximum) { lsdec = lsdec * base + digit; scale *= base; }
            else addExponent(step);
            addExponent(-static_cast<std::int32_t>(dot));
            ++cursor;
            c = static_cast<int>(digit);
            if (empty()) break;
            c = front();
            if (c == '_') {
                ++cursor;
                if (empty()) break;
                c = front();
            }
        }
        if (c == '.' && !dot) { ++cursor; dot += step; }
        else break;
    }
    if (!sawDigits) bail("no digits seen", 3360);
    if (hex && (empty() || (front() != 'p' && front() != 'P')))
        bail("Floating point parsing: exponent is required", 3363);
    if (hex || (!empty() && (front() == 'e' || front() == 'E'))) {
        ++cursor;
        if (empty()) fail("Unexpected end of input", 3378);
        bool minus = false;
        if (front() == '-' || front() == '+') minus = source[cursor++] == '-';
        sawDigits = false;
        std::int32_t magnitude = 0;
        while (!empty() && numeric::digit(front())) {
            if (magnitude < 0x7fffffff / 10 - 10) magnitude = magnitude * 10 + front() - '0';
            ++cursor;
            sawDigits = true;
        }
        addExponent(minus ? -magnitude : magnitude);
        if (!sawDigits) fail("No digits seen.", 3400);
    }
    // Actual real-format operations preserve the source floating environment.
    volatile numeric::Real value = static_cast<numeric::Real>(msdec);
    if (scale != 1) {
        const volatile numeric::Real convertedScale = static_cast<numeric::Real>(scale);
        const volatile numeric::Real product = value * convertedScale;
        const volatile numeric::Real convertedLow = static_cast<numeric::Real>(lsdec);
        value = product + convertedLow;
    }
    if (hex) value = numeric::sourceLdexp(static_cast<numeric::Real>(value), exponent);
    else if (value != 0) {
        std::size_t index = 0;
        int power = 4096;
        while (exponent > 0) {
            while (exponent >= power) { value = value * numeric::positive[index]; exponent -= power; }
            power >>= 1;
            ++index;
        }
        while (exponent < 0) {
            while (exponent <= -power) {
                value = value * numeric::negative[index];
                if (value == 0) fail("Range error", 3433);
                exponent += power;
            }
            power >>= 1;
            ++index;
        }
    }
    const volatile Target result = static_cast<Target>(negative ? -value : value);
    if (!std::isfinite(result)) fail("Range error", 3445);
    return result;
}
inline double firstPassDouble(std::string_view token) { return parseSourceFloating<double>(token); }

} // namespace nijigenerate::autorig::json::detail
