// Adapted from Phobos std.math.exponential, std.math.algebraic and floatTraits.
// Copyright The D Language Foundation 2000 - 2011.
// Based on CEPHES, Copyright (C) 2001 Stephen L. Moshier, incorporated by permission.
// Distributed under the Boost Software License 1.0; see geometry_kernel.LICENSE.
#include "detail/geometry_kernel.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace nijigenerate::autorig::deterministic::geometry::detail {
namespace {
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

constexpr std::array<double, 6> logP{
    7.70838733755885391666E0, 1.79368678507819816313E1,
    1.44989225341610930846E1, 4.70579119878881725854E0,
    4.97494994976747001425E-1, 1.01875663804580931796E-4};
constexpr std::array<double, 6> logQ{
    2.31251620126765340583E1, 7.11544750618563894466E1,
    8.29875266912776603211E1, 4.52279145837532221105E1,
    1.12873587189167450590E1, 1.00000000000000000000E0};
constexpr std::array<double, 3> logR{
    -6.41409952958715622951E1, 1.63866645699558079767E1, -7.89580278884799154124E-1};
constexpr std::array<double, 4> logS{
    -7.69691943550460008604E2, 3.12093766372244180303E2,
    -3.56722798256324312549E1, 1.00000000000000000000E0};

template<std::size_t N>
double polynomial(double x, const std::array<double, N>& coefficients) {
    double result = coefficients[N - 1];
    for (std::size_t i = 1; i < N; ++i) {
        result *= x;
        result += coefficients[N - 1 - i];
    }
    return result;
}

// This private path is only called for finite positive binary64 values.
double positiveFraction(double value, int& exponent) {
    auto bits = std::bit_cast<std::uint64_t>(value);
    int field = static_cast<int>((bits >> 52) & 0x7ff);
    if (field) {
        exponent = field - 1022;
    } else {
        value *= 0x1p52;
        bits = std::bit_cast<std::uint64_t>(value);
        field = static_cast<int>((bits >> 52) & 0x7ff);
        exponent = field - 1022 - 53 + 1;
    }
    return std::bit_cast<double>((bits & 0x800fffffffffffffULL) | 0x3fe0000000000000ULL);
}

double positiveLog(double x) {
    if (x == std::numeric_limits<double>::infinity()) return x;
    constexpr double c1 = 0.693359375;
    constexpr double c2 = -2.121944400546905827679e-4;
    // Smallest binary64 >= the configured real SQRT1_2: exact double-domain comparison.
    constexpr double rootHalfCeiling = 0x1.6a09e667f3bcdp-1;
    int exponent;
    x = positiveFraction(x, exponent);
    double y, z;
    if (exponent > 2 || exponent < -2) {
        if (x < rootHalfCeiling) {
            exponent -= 1;
            z = x - 0.5;
            y = 0.5 * z + 0.5;
        } else {
            z = x - 0.5;
            z -= 0.5;
            y = 0.5 * x + 0.5;
        }
        x = z / y;
        z = x * x;
        z = x * (z * polynomial(z, logR) / polynomial(z, logS));
        z += exponent * c2;
        z += x;
        z += exponent * c1;
        return z;
    }
    if (x < rootHalfCeiling) {
        exponent -= 1;
        x = 2.0 * x - 1.0;
    } else {
        x = x - 1.0;
    }
    z = x * x;
    y = x * (z * polynomial(x, logP) / polynomial(x, logQ));
    y += exponent * c2;
    z = y - 0.5 * z;
    z += x;
    z += exponent * c1;
    return z;
}
}

double kernel(double radius) {
    return radius > 0 ? 0.5 * radius * positiveLog(radius) : 0;
}
}
