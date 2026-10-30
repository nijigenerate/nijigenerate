#pragma once
// Phobos cutoff/power source: Copyright The D Language Foundation 2000-2011, 2019-.
// BSL-1.0; see port/licenses/phobos-float-format.txt.

#include "typed_format_float_bits.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <bit>
#include <cfenv>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <source_location>
#include <string>
#include <type_traits>

namespace nijigenerate::core::translation {

// Typed numeric layer, not a parser or a substitute for the full format engine.
// The source contract is LDC 1.41 Phobos std.format.internal.floats/write.
struct DecimalFormatSpec {
    char conversion = 's';
    std::optional<std::size_t> precision;
    std::size_t width = 0;
    bool left = false;
    bool plus = false;
    bool space = false;
    bool zero = false;
    bool alternate = false;
};

namespace detail {
// Selected Phobos std.math.exponential.pow(double, int) uses real intermediates.
inline double formatPowerTen(std::int32_t exponent) {
    // Keep the actual current rounding-mode arithmetic out of constant folding.
    volatile long double product = 1;
    volatile long double factor;
    auto remaining = static_cast<std::uint32_t>(exponent);
    if (exponent < 0) {
        if (exponent == -1) return 1.0 / 10.0;
        remaining = 0u - remaining;
        factor = product / 10.L;
    } else {
        switch (exponent) {
        case 0: return 1;
        case 1: return 10;
        case 2: return 10.0 * 10.0;
        default: break;
        }
        factor = 10;
    }
    for (;;) {
        if (remaining & 1u) product = product * factor;
        remaining >>= 1;
        if (!remaining) break;
        factor = factor * factor;
    }
    return static_cast<double>(product);
}

template<class T>
bool scientificGeneralFormat(T number, std::int32_t precision) {
    const auto magnitude = std::fabs(number);
    const auto lowerExponent = std::bit_cast<std::int32_t>(0u - 4u - static_cast<std::uint32_t>(precision));
    // Preserve source short-circuiting: the lower power is not always evaluated.
    switch (std::fegetround()) {
    case FE_UPWARD:
        return magnitude >= formatPowerTen(precision) - (number > 0 ? 1 : 0)
            || magnitude < 0.0001 - (number > 0 ? formatPowerTen(lowerExponent) : 0);
    case FE_DOWNWARD:
        return magnitude >= formatPowerTen(precision) - (number < 0 ? 1 : 0)
            || magnitude < 0.0001 - (number < 0 ? formatPowerTen(lowerExponent) : 0);
    case FE_TOWARDZERO:
        return magnitude >= formatPowerTen(precision) || magnitude < 0.0001;
    case FE_TONEAREST:
        return magnitude >= formatPowerTen(precision) - 0.5
            || magnitude < 0.0001 - 0.5 * formatPowerTen(lowerExponent);
    default: compat::ngAssertUnreachable("Unknown floating point rounding mode",
        "std/format/internal/floats.d", 2320);
    }
}

struct ExactFormatDecimal {
    std::string digits;
    std::size_t scale = 0;
    bool negative = false;
    bool infinity = false;
    bool nan = false;
};

inline void multiplyFormatDigits(std::string& digits, unsigned factor) {
    unsigned carry = 0;
    for (auto i = digits.size(); i != 0; --i) {
        const auto value = static_cast<unsigned>(digits[i - 1] - '0') * factor + carry;
        digits[i - 1] = static_cast<char>('0' + value % 10);
        carry = value / 10;
    }
    while (carry) {
        digits.insert(digits.begin(), static_cast<char>('0' + carry % 10));
        carry /= 10;
    }
}

template<FormatFloating T>
ExactFormatDecimal exactFormatDecimal(T value) {
    static_assert(std::numeric_limits<T>::is_iec559);
    using Bits = std::uint64_t;
    constexpr unsigned fractional = formatFractionalBits<T>;
    constexpr unsigned exponentMask = (1u << formatExponentBits<T>) - 1;
    constexpr int bias = formatExponentBias<T>;
    const auto bits = extractFormatFloatingBits(value);
    const auto exponent = bits.exponent;
    const auto fraction = bits.fraction;
    ExactFormatDecimal result;
    result.negative = bits.negative;
    if (exponent == exponentMask) {
        result.infinity = fraction == 0;
        result.nan = fraction != 0;
        return result;
    }
    const auto mantissa = exponent ? (Bits{1} << fractional) | fraction : fraction;
    if (!mantissa) { result.digits = "0"; return result; }
    char bytes[std::numeric_limits<Bits>::digits10 + 2];
    const auto converted = std::to_chars(bytes, bytes + sizeof(bytes), mantissa);
    result.digits.assign(bytes, converted.ptr);
    const auto power = static_cast<int>(exponent ? exponent : 1) - bias - static_cast<int>(fractional);
    if (power >= 0) {
        for (int i = 0; i < power; ++i) multiplyFormatDigits(result.digits, 2);
    } else {
        result.scale = static_cast<std::size_t>(-power);
        for (int i = 0; i < -power; ++i) multiplyFormatDigits(result.digits, 5);
    }
    return result;
}

inline bool incrementRoundedFormat(bool negative, bool remainder, char firstDiscarded,
    bool furtherNonzero, bool oddRetained) {
    switch (std::fegetround()) {
    case FE_UPWARD: return remainder && !negative;
    case FE_DOWNWARD: return remainder && negative;
    case FE_TOWARDZERO: return false;
    case FE_TONEAREST:
        return remainder && (firstDiscarded > '5' ||
            (firstDiscarded == '5' && (furtherNonzero || oddRetained)));
    default: compat::ngAssertUnreachable("Unknown floating point rounding mode",
        "std/format/internal/write.d", 3698);
    }
}

inline void incrementFormatInteger(std::string& digits) {
    for (auto i = digits.size(); i != 0; --i) {
        if (digits[i - 1] != '9') { ++digits[i - 1]; return; }
        digits[i - 1] = '0';
    }
    digits.insert(digits.begin(), '1');
}

// Round the exact decimal integer after retaining keep leading decimal digits.
// Negative keep accounts for implicit zeros before the first nonzero digit.
inline std::string roundFormatDigits(const ExactFormatDecimal& value, std::int64_t keep) {
    const auto& digits = value.digits;
    if (keep >= static_cast<std::int64_t>(digits.size())) {
        auto result = digits;
        result.append(static_cast<std::size_t>(keep) - digits.size(), '0');
        return result;
    }
    const auto retained = keep > 0 ? static_cast<std::size_t>(keep) : 0;
    auto result = retained ? digits.substr(0, retained) : std::string("0");
    const auto first = keep >= 0 ? digits[retained] : '0';
    const auto tailStart = keep >= 0 ? retained + 1 : 0;
    const bool tail = std::any_of(digits.begin() + tailStart, digits.end(), [](char c) { return c != '0'; });
    const bool remainder = first != '0' || tail;
    if (incrementRoundedFormat(value.negative, remainder, first, tail, ((result.back() - '0') & 1) != 0))
        incrementFormatInteger(result);
    return result;
}

inline std::string fixedFormatDigits(std::string digits, std::size_t fractional, bool alternate) {
    if (fractional) {
        if (digits.size() <= fractional) digits.insert(0, fractional - digits.size() + 1, '0');
        digits.insert(digits.size() - fractional, 1, '.');
    } else if (alternate) digits.push_back('.');
    return digits;
}

inline void trimFormatFraction(std::string& text) {
    const auto dot = text.find('.');
    if (dot == std::string::npos) return;
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
}

inline std::string formatDecimalExponent(int exponent, bool upper) {
    std::string result(1, upper ? 'E' : 'e');
    result.push_back(exponent < 0 ? '-' : '+');
    const auto magnitude = static_cast<unsigned>(exponent < 0 ? -exponent : exponent);
    char bytes[16];
    const auto converted = std::to_chars(bytes, bytes + sizeof(bytes), magnitude);
    if (converted.ptr - bytes < 2) result.push_back('0');
    result.append(bytes, converted.ptr);
    return result;
}

struct DecimalFormatParts {
    std::string prefix, integral, fraction, suffix;
    std::optional<std::int32_t> precision;
    bool general = false;
    bool special = false;
};

inline void trimDecimalFormatPart(DecimalFormatParts& result) {
    // Source retains the decimal point; writeAligned decides whether to put it.
    if (result.fraction.empty()) result.fraction = ".";
    while (result.fraction.size() > 1 && result.fraction.back() == '0')
        result.fraction.pop_back();
}

inline void fixedDecimalFormatParts(std::string digits, std::int64_t point, DecimalFormatParts& result) {
    if (point <= 0) {
        result.integral = "0";
        result.fraction = "." + std::string(static_cast<std::size_t>(-point), '0') + digits;
    } else if (static_cast<std::size_t>(point) >= digits.size()) {
        result.integral = std::move(digits);
        result.integral.append(static_cast<std::size_t>(point) - result.integral.size(), '0');
        result.fraction = ".";
    } else {
        result.integral = digits.substr(0, static_cast<std::size_t>(point));
        result.fraction = "." + digits.substr(static_cast<std::size_t>(point));
    }
    trimDecimalFormatPart(result);
}

// Produce the actual printFloatE/F/G components, before writeAligned emits
// precision padding. No completed field is formatted and split back apart.
template<FormatFloating T>
DecimalFormatParts makeDecimalFormatParts(T number, const DecimalFormatSpec& spec = {}) try {
    const auto value = exactFormatDecimal(number);
    const auto conversion = spec.conversion == 's' ? 'g' : spec.conversion;
    if (conversion != 'e' && conversion != 'E' && conversion != 'f' && conversion != 'F' &&
            conversion != 'g' && conversion != 'G')
        throw std::invalid_argument("Unsupported decimal floating point conversion");
    const bool upper = conversion == 'E' || conversion == 'F' || conversion == 'G';
    DecimalFormatParts result;
    result.prefix = value.negative ? "-" : spec.plus ? "+" : spec.space ? " " : "";
    result.general = conversion == 'g' || conversion == 'G';
    if (value.infinity || value.nan) {
        result.special = true;
        result.suffix = value.nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        return result;
    }
    auto requested = spec.precision.value_or(6);
    if (requested > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Decimal format precision overflows its source integer domain");
    if (result.general && requested == 0) requested = 1;
    result.precision = static_cast<std::int32_t>(requested);
    if (conversion == 'f' || conversion == 'F') {
        const auto keep = static_cast<std::int64_t>(value.digits.size())
            + static_cast<std::int64_t>(requested) - static_cast<std::int64_t>(value.scale);
        const bool discarded = keep < static_cast<std::int64_t>(value.digits.size());
        auto digits = roundFormatDigits(value, std::min(keep, static_cast<std::int64_t>(value.digits.size())));
        const auto scale = discarded ? requested : value.scale;
        const auto point = static_cast<std::int64_t>(digits.size()) - static_cast<std::int64_t>(scale);
        fixedDecimalFormatParts(std::move(digits), point, result);
        return result;
    }
    const auto significant = result.general ? requested : requested + 1;
    const bool generalScientific = result.general
        && scientificGeneralFormat(number, static_cast<std::int32_t>(significant));
    auto exponent = value.digits == "0" ? 0
        : static_cast<int>(value.digits.size()) - static_cast<int>(value.scale) - 1;
    const auto keep = std::min(significant, value.digits.size());
    auto digits = roundFormatDigits(value, static_cast<std::int64_t>(keep));
    if (digits.size() > keep) {
        digits.pop_back();
        ++exponent;
    }
    // printFloatE!true's zero case has no exponent even when G selected E.
    const bool scientific = !result.general || (generalScientific && value.digits != "0");
    if (scientific) {
        result.integral = digits.substr(0, 1);
        result.fraction = "." + digits.substr(1);
        trimDecimalFormatPart(result);
        result.suffix = formatDecimalExponent(exponent, upper);
    } else fixedDecimalFormatParts(std::move(digits), static_cast<std::int64_t>(exponent) + 1, result);
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

// Explicit f/F entry point with the actual signed FormatSpec precision. The
// negative branch's unsigned loop comparisons retain the entire fraction.
template<FormatFloating T>
DecimalFormatParts makeSignedFixedDecimalFormatParts(T number, DecimalFormatSpec spec,
    std::int32_t precision, bool general = false) try {
    const bool upper = spec.conversion == 'F' || spec.conversion == 'G';
    spec.conversion = general ? (upper ? 'G' : 'g') : (upper ? 'F' : 'f');
    if (precision >= 0) {
        spec.precision = static_cast<std::size_t>(precision);
        if constexpr (!FormatExtendedReal<T>) return makeDecimalFormatParts(number, spec);
        else {
            const auto bits = extractFormatFloatingBits(number);
            const auto exponent = bits.exponent ? static_cast<int>(bits.exponent) - formatExponentBias<T>
                : 1 - formatExponentBias<T>;
            if (exponent < 0 || exponent >= 64) return makeDecimalFormatParts(number, spec);
        }
    }
    const auto value = exactFormatDecimal(number);
    DecimalFormatParts result;
    result.general = general;
    result.prefix = value.negative ? "-" : spec.plus ? "+" : spec.space ? " " : "";
    if (value.infinity || value.nan) {
        result.special = true;
        result.suffix = value.nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        return result;
    }
    result.precision = precision;
    fixedDecimalFormatParts(value.digits,
        static_cast<std::int64_t>(value.digits.size()) - static_cast<std::int64_t>(value.scale), result);
    bool roundedMedium = false;
    if constexpr (FormatExtendedReal<T>) {
        const auto bits = extractFormatFloatingBits(number);
        int exponent = bits.exponent ? static_cast<int>(bits.exponent) - formatExponentBias<T>
            : 1 - formatExponentBias<T>;
        if (exponent >= 0 && exponent < 64) {
            const auto mantissa = bits.fraction | (bits.exponent ? (std::uint64_t{1} << 63) : 0);
            const auto integer = mantissa >> (63 - exponent);
            auto fraction = mantissa & ((std::uint64_t{1} << (63 - exponent)) - 1);
            std::uint64_t tail = 0;
            unsigned tailLength = 0;
            if (exponent < 3) {
                tailLength = static_cast<unsigned>(3 - exponent);
                tail = fraction & ((std::uint64_t{1} << tailLength) - 1);
                fraction >>= tailLength;
                exponent = 3;
            }
            char integerBytes[20];
            const auto end = std::to_chars(integerBytes, integerBytes + sizeof(integerBytes), integer);
            result.integral.assign(integerBytes, end.ptr);
            result.fraction = ".";
            auto significant = integer ? result.integral.size() : std::size_t{0};
            bool found = integer != 0;
            // floats.d:1800 tests the main fraction, not the residual tail.
            while (fraction != 0 && (general ? significant : result.fraction.size() - 1)
                    < static_cast<std::size_t>(precision)) {
                fraction *= 10;
                if (tailLength > 0) {
                    tail *= 5;
                    --tailLength;
                    fraction += tail >> tailLength;
                    if (tailLength > 0) tail &= (std::uint64_t{1} << tailLength) - 1;
                }
                result.fraction.push_back(static_cast<char>('0' + (fraction >> (63 - exponent))));
                if (found || result.fraction.back() != '0') { found = true; ++significant; }
                fraction &= (std::uint64_t{1} << (63 - exponent)) - 1;
            }
            const bool remainder = fraction != 0;
            unsigned nextDigit = 0;
            if (remainder) {
                // Source's rounding lookahead deliberately omits tail merging.
                fraction *= 10;
                nextDigit = static_cast<unsigned>(fraction >> (63 - exponent));
                fraction &= (std::uint64_t{1} << (63 - exponent)) - 1;
            }
            const auto last = result.fraction.size() > 1 ? result.fraction.back() : result.integral.back();
            if (incrementRoundedFormat(value.negative, remainder, static_cast<char>('0' + nextDigit),
                    fraction != 0, ((last - '0') & 1) != 0)) {
                bool carry = true;
                for (auto i = result.fraction.size(); carry && i > 1;) {
                    auto& digit = result.fraction[--i];
                    if (digit == '9') digit = '0';
                    else { ++digit; carry = false; }
                }
                if (carry) incrementFormatInteger(result.integral);
            }
            trimDecimalFormatPart(result);
            roundedMedium = true;
        }
    }
    // Source invokes round even for RoundingClass.ZERO, including its mode
    // dispatch. No digits change in this fully consumed negative-F branch.
    if (!roundedMedium) (void)incrementRoundedFormat(value.negative, false, '0', false, false);
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

inline std::string alignFormatDecimal(std::string body, bool negative, bool special, const DecimalFormatSpec& spec) {
    std::string sign = negative ? "-" : spec.plus ? "+" : spec.space ? " " : "";
    const auto length = sign.size() + body.size();
    const auto padding = spec.width > length ? spec.width - length : 0;
    if (spec.left) return sign + body + std::string(padding, ' ');
    if (spec.zero && !special) return sign + std::string(padding, '0') + body;
    return std::string(padding, ' ') + sign + body;
}
} // namespace detail

template<detail::FormatFloating T>
std::string ngFormatDecimal(T number, const DecimalFormatSpec& spec = {}) try {
    auto value = detail::exactFormatDecimal(number);
    const auto conversion = spec.conversion == 's' ? 'g' : spec.conversion;
    const bool upper = conversion == 'E' || conversion == 'F' || conversion == 'G';
    if (conversion != 'e' && conversion != 'E' && conversion != 'f' && conversion != 'F' &&
        conversion != 'g' && conversion != 'G')
        throw std::invalid_argument("Unsupported decimal floating point conversion");
    if (value.infinity || value.nan) {
        const auto body = value.nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        return detail::alignFormatDecimal(body, value.negative, true, spec);
    }
    const auto requested = spec.precision.value_or(6);
    if (requested > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Decimal format precision overflows its source integer domain");
    std::string body;
    if (conversion == 'f' || conversion == 'F') {
        const auto keep = static_cast<std::int64_t>(value.digits.size()) +
            static_cast<std::int64_t>(requested) - static_cast<std::int64_t>(value.scale);
        body = detail::fixedFormatDigits(detail::roundFormatDigits(value, keep), requested, spec.alternate);
    } else {
        const bool general = conversion == 'g' || conversion == 'G';
        const auto significant = general ? std::max<std::size_t>(requested, 1) : requested + 1;
        const bool generalScientific = general &&
            detail::scientificGeneralFormat(number, static_cast<std::int32_t>(significant));
        int exponent = value.digits == "0" ? 0 :
            static_cast<int>(value.digits.size()) - static_cast<int>(value.scale) - 1;
        auto digits = detail::roundFormatDigits(value, static_cast<std::int64_t>(significant));
        if (digits.size() > significant) { digits.pop_back(); ++exponent; }
        // Phobos's general zero path prints no exponent even when it selects E.
        const bool scientific = !general || (generalScientific && number != 0);
        if (scientific) {
            body = digits.substr(0, 1);
            if (digits.size() > 1 || spec.alternate) body.push_back('.');
            body.append(digits.substr(1));
            if (general && !spec.alternate) detail::trimFormatFraction(body);
            body += detail::formatDecimalExponent(exponent, upper);
        } else {
            const int point = exponent + 1;
            if (point <= 0) body = "0." + std::string(static_cast<std::size_t>(-point), '0') + digits;
            else if (static_cast<std::size_t>(point) >= digits.size()) {
                body = digits + std::string(static_cast<std::size_t>(point) - digits.size(), '0');
                if (spec.alternate) body.push_back('.');
            } else {
                body = digits;
                body.insert(static_cast<std::size_t>(point), 1, '.');
            }
            if (!spec.alternate) detail::trimFormatFraction(body);
        }
    }
    return detail::alignFormatDecimal(std::move(body), value.negative, false, spec);
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

} // namespace nijigenerate::core::translation
