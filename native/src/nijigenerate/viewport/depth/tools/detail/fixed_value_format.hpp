#pragma once

#include <boost/multiprecision/cpp_int.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::viewport::depth::tools::operation::detail {

// Internal implementation of the source's literal "%.2f" for IEEE float32.
// Phobos uses sign bits and exact decimal rounding, independent of locale.
inline std::string formatDepthValue(float value) try {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    using boost::multiprecision::cpp_int;
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const bool negative = (bits >> 31) != 0;
    const auto exponentBits = (bits >> 23) & 0xff;
    const auto fraction = bits & 0x7fffff;
    if (exponentBits == 0xff) {
        return std::string(negative ? "-" : "") + (fraction == 0 ? "inf" : "nan");
    }
    const std::uint32_t significand = exponentBits == 0 ? fraction : fraction | 0x800000;
    const int exponent = exponentBits == 0 ? -149 : static_cast<int>(exponentBits) - 150;
    cpp_int numerator = static_cast<std::uint64_t>(significand) * 100;
    cpp_int rounded;
    if (exponent >= 0) {
        rounded = numerator << exponent;
    } else {
        const auto shift = static_cast<unsigned>(-exponent);
        rounded = numerator >> shift;
        const cpp_int remainder = numerator - (rounded << shift);
        if (remainder != 0) {
            const int mode = std::fegetround();
            bool increment = false;
            if (mode == FE_UPWARD) {
                increment = !negative;
            } else if (mode == FE_DOWNWARD) {
                increment = negative;
            } else if (mode != FE_TOWARDZERO) {
                const cpp_int denominator = cpp_int(1) << shift;
                const cpp_int twice = remainder << 1;
                increment = twice > denominator || (twice == denominator && (rounded & 1) != 0);
            }
            if (increment) ++rounded;
        }
    }
    std::string result;
    do {
        const unsigned digit = (rounded % 10).convert_to<unsigned>();
        result.push_back(static_cast<char>('0' + digit));
        rounded /= 10;
    } while (rounded != 0);
    std::reverse(result.begin(), result.end());
    if (result.size() < 3) result.insert(0, 3 - result.size(), '0');
    result.insert(result.end() - 2, '.');
    if (negative) result.insert(result.begin(), '-');
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

} // namespace nijigenerate::viewport::depth::tools::operation::detail
