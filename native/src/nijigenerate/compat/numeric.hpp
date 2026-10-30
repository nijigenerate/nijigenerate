#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace nijigenerate::compat {

// Matches the validated LDC 1.41 x86-64 float-to-int contract, including invalid inputs.
inline std::int32_t ngFloatToInt32(double value) {
    const double integral = std::trunc(value);
    if (!std::isfinite(integral) || integral < -2147483648.0 || integral >= 2147483648.0)
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(integral);
}

} // namespace nijigenerate::compat
