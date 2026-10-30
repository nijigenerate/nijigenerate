#pragma once
// Adapted from LDC 1.41 Phobos std/math/exponential.d's IEEE double branches.
// Copyright The D Language Foundation 2000-2011; CEPHES Copyright Stephen L. Moshier 2001.
// Distributed under Boost Software License 1.0; see ../geometry_kernel.LICENSE.
#include <array>
#include <cmath>
#include <limits>
namespace nijigenerate::autorig::deterministic::depth::detail {
template<std::size_t N> double polynomial(double x, const std::array<double, N>& a) {
    double r = a[N - 1];
    for (std::size_t i = 1; i < N; ++i) { r *= x; r += a[N - 1 - i]; }
    return r;
}
inline double log2(double x) {
    constexpr std::array<double, 6> p{7.70838733755885391666E0, 1.79368678507819816313E1,
        1.44989225341610930846E1, 4.70579119878881725854E0, 4.97494994976747001425E-1, 1.01875663804580931796E-4};
    constexpr std::array<double, 6> q{2.31251620126765340583E1, 7.11544750618563894466E1,
        8.29875266912776603211E1, 4.52279145837532221105E1, 1.12873587189167450590E1, 1.};
    constexpr std::array<double, 3> r{-6.41409952958715622951E1, 1.63866645699558079767E1, -7.89580278884799154124E-1};
    constexpr std::array<double, 4> s{-7.69691943550460008604E2, 3.12093766372244180303E2, -3.56722798256324312549E1, 1.};
    if (std::isnan(x) || (std::isinf(x) && !std::signbit(x))) return x;
    if (x == 0) return -std::numeric_limits<double>::infinity();
    if (x < 0) return std::numeric_limits<double>::quiet_NaN();
    int exponent; x = std::frexp(x, &exponent); double y, z;
    if (exponent > 2 || exponent < -2) {
        if (x < 0x1.6a09e667f3bcdp-1) { --exponent; z = x - .5; y = .5 * z + .5; }
        else { z = x - .5; z -= .5; y = .5 * x + .5; }
        x = z / y; z = x * x; y = x * (z * polynomial(z, r) / polynomial(z, s));
    } else {
        if (x < 0x1.6a09e667f3bcdp-1) { --exponent; x = 2. * x - 1.; } else x = x - 1.;
        z = x * x; y = x * (z * polynomial(x, p) / polynomial(x, q)); y = y - .5 * z;
    }
    // The MSVC-target real LOG2E constant is rounded before the subtraction.
    // Rounding a separately written LOG2E-1 decimal gives a different value.
    constexpr double log2eMinusOne = 0x1.71547652b82fep+0 - 1.0;
    z = y * log2eMinusOne; z += x * log2eMinusOne; z += y; z += x; z += exponent; return z;
}
inline double exp2(double x) {
    constexpr std::array<double, 3> p{1.51390680115615096133E3, 2.02020656693165307700E1, 2.30933477057345225087E-2};
    constexpr std::array<double, 3> q{4.36821166879210612817E3, 2.33184211722314911771E2, 1.};
    if (std::isnan(x)) return x;
    if (x > 1024) return std::numeric_limits<double>::infinity();
    if (x < -1022) return 0.;
    const double i = std::floor(x + .5); int n = static_cast<int>(i); x -= i;
    const double xx = x * x, px = x * polynomial(xx, p);
    x = px / (polynomial(xx, q) - px); x = 1. + 2. * x;
    return std::ldexp(x, n);
}
}
