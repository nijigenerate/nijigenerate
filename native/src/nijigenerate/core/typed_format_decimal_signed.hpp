#pragma once
// Phobos signed scientific precision: std/format/internal/floats.d595-1078.
// BSL-1.0; retained notices: port/licenses/phobos-float-format.txt.
#include "typed_format_decimal.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <array>

namespace nijigenerate::core::translation::detail {

template<std::size_t N>
struct ScientificFormatBuffer {
    std::array<unsigned char, N> bytes;
    ScientificFormatBuffer() { bytes.fill(0xff); } // D char.init.
    unsigned char& at(std::size_t index, unsigned line,
        const char* file = "std/format/internal/floats.d") {
        if (index >= N)
            throw compat::ngStaticError<compat::ArrayIndexError>(index, N,
                compat::RetainedString::fromBorrowedView(file), line);
        return bytes[index];
    }
    std::string slice(std::size_t lower, std::size_t upper, unsigned line) {
        if (lower > upper || upper > N)
            throw compat::ngStaticError<compat::ArraySliceError>(lower, upper, N,
                compat::RetainedString::fromStaticLiteral("std/format/internal/floats.d"), line);
        return std::string(reinterpret_cast<const char*>(bytes.data() + lower), upper - lower);
    }
};

template<std::size_t N>
bool roundScientificFormatBuffer(ScientificFormatBuffer<N>& buffer, std::size_t left,
    std::size_t right, char rounding, bool negative) {
#ifndef NDEBUG
    const auto require = [](bool condition, unsigned line) {
        if (!condition)
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("Assertion failure"),
                compat::RetainedString::fromStaticLiteral("std/format/internal/write.d"), line);
    };
    require(left < N, 3669);
    require(right <= N, 3671);
    require(right >= left, 3672);
#endif
    bool up;
    switch (std::fegetround()) {
    case FE_UPWARD: up = rounding != '0' && !negative; break;
    case FE_DOWNWARD: up = rounding != '0' && negative; break;
    case FE_TOWARDZERO: up = false; break;
    case FE_TONEAREST:
        up = rounding == '9';
        if (rounding == '5') {
            auto last = buffer.at(right - 1, 3723, "std/format/internal/write.d");
            if (last == '.') last = buffer.at(right - 2, 3724, "std/format/internal/write.d");
            up = (last <= '9' && last % 2 != 0) || (last > '9' && last % 2 == 0);
        }
        break;
    default: compat::ngAssertUnreachable("Unknown floating point rounding mode",
        "std/format/internal/write.d", 3698);
    }
    if (!up) return false;
    for (auto i = right; i > left;) {
        --i;
        if (buffer.at(i, 3734, "std/format/internal/write.d") == '.') continue;
        if (buffer.at(i, 3735, "std/format/internal/write.d") == '9')
            buffer.at(i, 3736, "std/format/internal/write.d") = '0';
        else {
            ++buffer.at(i, 3742, "std/format/internal/write.d");
            return false;
        }
    }
    buffer.at(left - 1, 3747, "std/format/internal/write.d") = '1';
    return true;
}

// All negative precision branches, and the real64 medium-C branch with either
// precision sign. Positive A/B are supplied by the ordinary decimal producer.
template<FormatFloating T>
DecimalFormatParts makeScientificDecimalBufferParts(T number, const DecimalFormatSpec& spec,
    std::int32_t precision, bool general = false) try {
    const auto value = exactFormatDecimal(number);
    const bool upper = spec.conversion == 'E' || spec.conversion == 'G';
    DecimalFormatParts result;
    result.prefix = value.negative ? "-" : spec.plus ? "+" : spec.space ? " " : "";
    result.general = general;
    if (value.infinity || value.nan) {
        result.special = true;
        result.suffix = value.nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        return result;
    }
    result.precision = precision;
    if (value.digits == "0") {
        result.integral = "0";
        result.fraction = ".";
        if (!general) result.suffix = upper ? "E+00" : "e+00";
        return result;
    }
    constexpr unsigned mantissaBits = formatFractionalBits<T> + 1;
    constexpr std::size_t capacity = mantissaBits + formatExponentBias<T> + 1;
    constexpr int smallBound = FormatExtendedReal<T> ? 0 : static_cast<int>(mantissaBits) - 61;
    ScientificFormatBuffer<capacity> buffer;
    const auto bits = extractFormatFloatingBits(number);
    int exponent = bits.exponent ? static_cast<int>(bits.exponent) - formatExponentBias<T>
        : 1 - formatExponentBias<T>;
    const auto mantissa = bits.fraction |
        (bits.exponent ? (std::uint64_t{1} << (mantissaBits - 1)) : 0);
    std::size_t left = 1, right = 1, start = 1;
    int finalExponent = 0;
    char rounding = '0';
    if (exponent >= static_cast<int>(mantissaBits)) {
        right = capacity;
        left = right - value.digits.size();
        for (std::size_t i = 0; i < value.digits.size(); ++i)
            buffer.at(left + i, 756) = static_cast<unsigned char>(value.digits[i]);
        finalExponent = static_cast<int>(value.digits.size()) - 1;
        start = left + static_cast<std::size_t>(precision) + (general ? 0 : 1);
        buffer.at(left - 1, 767) = buffer.at(left, 767);
        buffer.at(left, 768) = '.';
        --left;
        if (start < right) {
            const auto first = buffer.at(start, 774);
            rounding = first == '0' ? '0' : first == '5' ? '5' : first > '5' ? '9' : '1';
            if (rounding == '0' || rounding == '5') {
                for (auto i = start + 1; i < right; ++i)
                    if (buffer.at(i, 780) > '0') {
                        rounding = rounding == '5' ? '9' : '1';
                        break;
                    }
            }
            right = start;
        }
    } else if (exponent < smallBound) {
        auto digits = value.digits;
        while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
        finalExponent = static_cast<int>(value.digits.size()) - static_cast<int>(value.scale) - 1;
        buffer.at(right++, 848) = static_cast<unsigned char>(digits[0]);
        buffer.at(right++, 849) = '.';
        for (std::size_t i = 1; i < digits.size(); ++i)
            buffer.at(right++, 871) = static_cast<unsigned char>(digits[i]);
    } else {
        auto integer = mantissa >> (mantissaBits - 1 - exponent);
        auto fraction = mantissa & ((std::uint64_t{1} << (mantissaBits - 1 - exponent)) - 1);
        std::uint64_t tail = 0;
        unsigned tailLength = 0;
        if constexpr (FormatExtendedReal<T>) {
            if (exponent < 3) {
                tailLength = static_cast<unsigned>(3 - exponent);
                tail = fraction & ((std::uint64_t{1} << tailLength) - 1);
                fraction >>= tailLength;
                exponent = 3;
            }
        }
        const auto next = [&] {
            fraction *= 10;
            if (tailLength > 0) {
                tail *= 5;
                --tailLength;
                fraction += tail >> tailLength;
                if (tailLength > 0) tail &= (std::uint64_t{1} << tailLength) - 1;
            }
            const auto digit = fraction >> (mantissaBits - 1 - exponent);
            fraction &= (std::uint64_t{1} << (mantissaBits - 1 - exponent)) - 1;
            return static_cast<unsigned char>('0' + digit);
        };
        start = 0;
        bool found = false;
        if (integer > 0) {
            left = right = (std::bit_width(integer) - 1) * 100 / 332 + 4;
            while (integer >= 10) {
                buffer.at(--left, 926) = static_cast<unsigned char>('0' + integer % 10);
                integer /= 10;
                ++finalExponent;
                ++start;
            }
            buffer.at(--left, 932) = '.';
            buffer.at(--left, 933) = static_cast<unsigned char>('0' + integer);
            const auto limit = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(precision) + (general ? 1u : 2u));
            if (right - left > static_cast<std::size_t>(limit)) {
                const auto oldRight = right;
                right = left + static_cast<std::size_t>(limit);
                const auto first = buffer.at(right, 945);
                rounding = first == '5' ? '5' : first == '0' ? '0' : first > '5' ? '9' : '1';
                if (first == '0' || first == '5') {
                    if (fraction != 0) rounding = first == '5' ? '9' : '1';
                    else for (auto i = right + 1; i < oldRight; ++i)
                        if (buffer.at(i, 952) > '0') {
                            rounding = first == '5' ? '9' : '1';
                            break;
                        }
                }
                found = true;
            }
        } else {
            while (fraction != 0) {
                --finalExponent;
                const auto digit = next();
                if (digit > '0') {
                    buffer.at(right++, 987) = digit;
                    buffer.at(right++, 988) = '.';
                    break;
                }
            }
        }
        const auto limit = static_cast<std::size_t>(std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(precision) - (general ? 1u : 0u)));
        while (fraction != 0 && start < limit) {
            buffer.at(right++, 1018) = next();
            ++start;
        }
        if (fraction != 0 && !found) {
            fraction *= 10;
            const auto digit = fraction >> (mantissaBits - 1 - exponent);
            fraction &= (std::uint64_t{1} << (mantissaBits - 1 - exponent)) - 1;
            rounding = digit == 5 && fraction == 0 ? '5' : digit >= 5 ? '9' : '1';
        }
    }
    if (roundScientificFormatBuffer(buffer, left, right, rounding, value.negative)) {
        --left;
        --right;
        buffer.at(left + 2, 1048) = buffer.at(left + 1, 1048);
        buffer.at(left + 1, 1049) = '.';
        ++finalExponent;
    }
    while (right > left + 1 && buffer.at(right - 1, 1069) == '0') --right;
    if (right == left + 1) buffer.at(right++, 1072) = '.';
    result.integral = buffer.slice(left, left + 1, general ? 1075 : 1078);
    result.fraction = buffer.slice(left + 1, right, general ? 1075 : 1078);
    result.suffix = formatDecimalExponent(finalExponent, upper);
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

// Caller resolves UNSPECIFIED/DYNAMIC and selects the negative precision branch.
// General G's signed cutoff selects this E body before entering this function.
template<FormatFloating T>
DecimalFormatParts makeNegativeScientificDecimalFormatParts(T number, const DecimalFormatSpec& spec,
    std::int32_t precision, bool general = false) {
    return makeScientificDecimalBufferParts(number, spec, precision, general);
}

// Explicit e/E entry point. Keep real64 medium-C's authored tail control for
// nonnegative precision too, rather than replacing it with exact-value digits.
template<FormatFloating T>
DecimalFormatParts makeSignedScientificDecimalFormatParts(T number, DecimalFormatSpec spec,
    std::int32_t precision) {
    spec.conversion = spec.conversion == 'E' ? 'E' : 'e';
    if (precision < 0) return makeScientificDecimalBufferParts(number, spec, precision);
    if constexpr (FormatExtendedReal<T>) {
        const auto bits = extractFormatFloatingBits(number);
        const auto exponent = bits.exponent ? static_cast<int>(bits.exponent) - formatExponentBias<T>
            : 1 - formatExponentBias<T>;
        if (exponent >= 0 && exponent < 64)
            return makeScientificDecimalBufferParts(number, spec, precision);
    }
    spec.precision = static_cast<std::size_t>(precision);
    return makeDecimalFormatParts(number, spec);
}

template<FormatFloating T>
DecimalFormatParts makeNegativeGeneralDecimalFormatParts(T number, DecimalFormatSpec spec,
    std::int32_t precision) {
    const auto bits = extractFormatFloatingBits(number);
    constexpr auto specialExponent = (1u << formatExponentBits<T>) - 1;
    // printFloat handles special values before printFloatG's rounding cutoff.
    if (bits.exponent == specialExponent || scientificGeneralFormat(number, precision))
        return makeNegativeScientificDecimalFormatParts(number, spec, precision, true);
    return makeSignedFixedDecimalFormatParts(number, spec, precision, true);
}

template<FormatFloating T>
DecimalFormatParts makeSignedGeneralDecimalFormatParts(T number, DecimalFormatSpec spec,
    std::int32_t precision) {
    spec.conversion = spec.conversion == 'G' ? 'G' : 'g';
    if (precision < 0) return makeNegativeGeneralDecimalFormatParts(number, spec, precision);
    if (precision == 0) precision = 1;
    if constexpr (FormatExtendedReal<T>) {
        const auto bits = extractFormatFloatingBits(number);
        const auto exponent = bits.exponent ? static_cast<int>(bits.exponent) - formatExponentBias<T>
            : 1 - formatExponentBias<T>;
        if (exponent >= 0 && exponent < 64) {
            if (scientificGeneralFormat(number, precision))
                return makeScientificDecimalBufferParts(number, spec, precision, true);
            return makeSignedFixedDecimalFormatParts(number, spec, precision, true);
        }
    }
    spec.precision = static_cast<std::size_t>(precision);
    return makeDecimalFormatParts(number, spec);
}

// The caller supplies the resolved precision (six for unspecified decimal
// conversions). No negative precision is mapped to an unsigned specification.
template<FormatFloating T>
DecimalFormatParts makeSignedDecimalFormatParts(T number, const DecimalFormatSpec& spec,
    std::int32_t precision) {
    switch (spec.conversion) {
    case 'e': case 'E': return makeSignedScientificDecimalFormatParts(number, spec, precision);
    case 'f': case 'F': return makeSignedFixedDecimalFormatParts(number, spec, precision);
    case 's': case 'g': case 'G': return makeSignedGeneralDecimalFormatParts(number, spec, precision);
    default: return makeDecimalFormatParts(number, spec);
    }
}

} // namespace nijigenerate::core::translation::detail
