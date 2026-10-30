#pragma once

// Hexadecimal formatting contract: LDC 1.41 Phobos internal/floats.d.
// Copyright The D Language Foundation 2019-, Bernhard Seckinger.
// Distributed under the Boost Software License, Version 1.0; the full license
// notice is retained in the included typed_format_layout.hpp source header.
#include "typed_format_decimal.hpp"
#include "typed_format_layout.hpp"

namespace nijigenerate::core::translation::detail {

struct HexFormatParts {
    std::string prefix;
    std::string integral;
    std::string fraction;
    std::string suffix;
    std::optional<std::int32_t> precision;
    bool special = false;
};

inline unsigned formatHexDigit(char digit) {
    if (digit <= '9') return static_cast<unsigned>(digit - '0');
    return static_cast<unsigned>((digit >= 'a' ? digit - 'a' : digit - 'A') + 10);
}
inline char formatHexCharacter(unsigned value, bool upper) {
    return static_cast<char>(value < 10 ? '0' + value : (upper ? 'A' : 'a') + value - 10);
}

template<FormatFloating T>
HexFormatParts makeHexFormatComponents(T value, const DecimalFormatSpec& spec, bool padFraction) try {
    constexpr unsigned fractional = formatFractionalBits<T>;
    constexpr int bias = formatExponentBias<T>;
    constexpr unsigned alignedBits = (fractional + 3) / 4 * 4;
    constexpr unsigned hexDigits = alignedBits / 4;
    constexpr unsigned exponentMask = (1u << formatExponentBits<T>) - 1;
    const auto bits = extractFormatFloatingBits(value);
    const auto exponentField = bits.exponent;
    const auto fractionField = bits.fraction;
    const bool negative = bits.negative;
    const bool upper = spec.conversion == 'A';
    HexFormatParts result;
    result.prefix = negative ? "-" : spec.plus ? "+" : spec.space ? " " : "";
    if (exponentField == exponentMask) {
        result.special = true;
        result.suffix = fractionField ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        return result;
    }
    result.prefix += upper ? "0X" : "0x";
    int exponent = exponentField ? static_cast<int>(exponentField) - bias : 1 - bias;
    if (!exponentField && !fractionField) exponent = 0;
    result.integral = exponentField ? "1" : "0";
    const auto aligned = fractionField << (alignedBits - fractional);
    std::string digits;
    for (unsigned i = 0; i < hexDigits; ++i) {
        const auto shift = (hexDigits - 1 - i) * 4;
        digits.push_back(formatHexCharacter(static_cast<unsigned>((aligned >> shift) & 15), upper));
    }
    while (!digits.empty() && digits.back() == '0') digits.pop_back();
    const auto precision = spec.precision.value_or(digits.size());
    if (precision > static_cast<std::size_t>(INT_MAX))
        throw std::length_error("Hex format precision exceeds source integer domain");
    result.precision = static_cast<std::int32_t>(precision);
    if (precision < digits.size()) {
        const auto first = formatHexDigit(digits[precision]);
        const bool tail = std::any_of(digits.begin() + precision + 1, digits.end(),
            [](char digit) { return digit != '0'; });
        const auto retained = precision ? formatHexDigit(digits[precision - 1]) :
            static_cast<unsigned>(result.integral[0] - '0');
        const char roundingClass = first < 8 ? (first ? '1' : '0') : first == 8 ? '5' : '9';
        auto rounded = result.integral + digits.substr(0, precision);
        if (incrementRoundedFormat(negative, first != 0 || tail, roundingClass, tail, (retained & 1) != 0)) {
            for (auto i = rounded.size(); i != 0; --i) {
                const auto digit = formatHexDigit(rounded[i - 1]);
                if (digit != 15) { rounded[i - 1] = formatHexCharacter(digit + 1, upper); break; }
                rounded[i - 1] = '0';
            }
        }
        result.integral.assign(rounded, 0, 1);
        digits = rounded.substr(1);
    } else if (padFraction) digits.append(precision - digits.size(), '0');
    if (!padFraction || !digits.empty() || spec.alternate) result.fraction = "." + digits;
    result.suffix = upper ? "P" : "p";
    result.suffix += exponent < 0 ? '-' : '+';
    char exponentBytes[16];
    const auto magnitude = static_cast<unsigned>(exponent < 0 ? -exponent : exponent);
    const auto converted = std::to_chars(exponentBytes, exponentBytes + sizeof(exponentBytes), magnitude);
    result.suffix.append(exponentBytes, converted.ptr);
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

template<FormatFloating T>
HexFormatParts makeHexFormatParts(T value, const DecimalFormatSpec& spec) {
    return makeHexFormatComponents(value, spec, true);
}

// The Source precision is signed even though the public collecting numeric
// specification uses size_t. At floats.d:164 a negative precision converts to
// size_t for the comparison, so the complete mantissa reaches writeAligned.
template<FormatFloating T>
HexFormatParts makeSignedHexFormatParts(T value, DecimalFormatSpec spec, std::int32_t precision) {
    if (precision >= 0) {
        spec.precision = static_cast<std::size_t>(precision);
        return makeHexFormatComponents(value, spec, false);
    }
    spec.precision.reset();
    auto result = makeHexFormatComponents(value, spec, false);
    if (!result.special) result.precision = precision;
    return result;
}

// Source raw conversions preserve native representation, optionally selecting
// big/little endian with the same plus/dash flag rule. No numeric arithmetic.
inline std::string formatRawBits(std::uint64_t bits, unsigned width, bool plus, bool dash) {
    if (!width || width > 64 || width % CHAR_BIT)
        throw std::invalid_argument("Unsupported raw bit width");
    static_assert(CHAR_BIT == 8);
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
    std::string result;
    for (unsigned i = 0; i < width / 8; ++i)
        result.push_back(static_cast<char>((bits >> (i * 8)) & 0xff));
    if constexpr (std::endian::native == std::endian::big) std::reverse(result.begin(), result.end());
    const bool swap = (std::endian::native == std::endian::little && plus) ||
        (std::endian::native == std::endian::big && dash);
    if (swap) std::reverse(result.begin(), result.end());
    return result;
}

} // namespace nijigenerate::core::translation::detail
