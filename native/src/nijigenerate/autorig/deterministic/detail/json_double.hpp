#pragma once

// Required finite-double %.18g policy of Phobos internal/floats.d and write.d.
// Copyright The D Language Foundation 2019-. BSL-1.0: phobos-rig-json.txt.
#include <nijigenerate/core/typed_format_decimal.hpp>
#include "../contracts_file.hpp"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <string>
#if defined(_MSC_VER) && defined(_M_X64)
#include <immintrin.h>
#pragma intrinsic(_fxsave)
#pragma float_control(precise, on, push)
#endif

namespace nijigenerate::autorig::deterministic::contracts::detail {

inline int jsonRoundingMode() {
    // Configured LDC FloatingPointControl reads x87, including on Windows x64.
#if defined(_MSC_VER) && defined(_M_X64)
    alignas(16) unsigned char state[512]{};
    _fxsave(state);
    const unsigned word = state[0] | (static_cast<unsigned>(state[1]) << 8);
#elif defined(__i386__) || defined(__x86_64__)
    std::uint16_t word;
    __asm__ __volatile__("fnstcw %0" : "=m"(word));
#else
    return std::fegetround();
#endif
#if (defined(_MSC_VER) && defined(_M_X64)) || defined(__i386__) || defined(__x86_64__)
    switch (word & 0x0c00) {
    case 0x0400: return FE_DOWNWARD;
    case 0x0800: return FE_UPWARD;
    case 0x0c00: return FE_TOWARDZERO;
    default: return FE_TONEAREST;
    }
#endif
}

inline std::string jsonFiniteDouble(double value) {
    // Reuse the actual production binary64-to-exact-decimal integer provider.
    auto exact = core::translation::detail::exactFormatDecimal(value);
    const int mode = jsonRoundingMode();
    const double magnitude = std::fabs(value);
    // LLVM powi(10,18) and powi(10,22) have exact binary64 integer results.
    volatile double ten18 = 1e18;
    double largeDelta = 0;
    switch (mode) {
    case FE_UPWARD:
        largeDelta = value > 0 ? 1 : 0;
        break;
    case FE_DOWNWARD:
        largeDelta = value < 0 ? 1 : 0;
        break;
    case FE_TOWARDZERO: break;
    default: largeDelta = 0.5; break;
    }
    // Keep Phobos's pre-round E/F selection, including its boundary gaps.
    const auto smallBranch = [&] {
        const auto powerMinus22 = [] {
            volatile double denominator = 1e22;
            return 1.0 / denominator;
        };
        double delta = 0;
        switch (mode) {
        case FE_UPWARD: delta = value > 0 ? powerMinus22() : 0; break;
        case FE_DOWNWARD: delta = value < 0 ? powerMinus22() : 0; break;
        case FE_TOWARDZERO: break;
        default: delta = 0.5 * powerMinus22(); break;
        }
        volatile double small = 0.0001;
        return magnitude < small - delta;
    };
    const bool exponential = magnitude >= ten18 - largeDelta || smallBranch();
    if (exact.digits == "0") return allocateContractStorage([&] {
        return std::string(exact.negative ? "-0.0" : "0.0");
    });
    int exponent = static_cast<int>(exact.digits.size()) - static_cast<int>(exact.scale) - 1;
    auto digits = allocateContractStorage([&] { return exact.digits.substr(0, 18); });
    if (exact.digits.size() > 18) {
        const char first = exact.digits[18];
        const bool tail = std::any_of(exact.digits.begin() + 19, exact.digits.end(),
            [](char digit) { return digit != '0'; });
        const bool remainder = first != '0' || tail;
        bool increment = false;
        switch (mode) {
        case FE_UPWARD: increment = remainder && !exact.negative; break;
        case FE_DOWNWARD: increment = remainder && exact.negative; break;
        case FE_TOWARDZERO: break;
        default:
            increment = first > '5' || (first == '5' && (tail || ((digits.back() - '0') & 1)));
            break;
        }
        if (increment) {
            core::translation::detail::incrementFormatInteger(digits);
            if (digits.size() > 18) ++exponent;
        }
    }
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    auto result = allocateContractStorage([&] { return std::string(exact.negative ? "-" : ""); });
    if (exponential) {
        allocateContractStorage([&] { result += digits[0]; });
        if (digits.size() > 1) allocateContractStorage([&] {
            result += '.'; result.append(digits, 1, std::string::npos);
        });
        const auto suffix = core::translation::detail::formatDecimalExponent(exponent, false);
        allocateContractStorage([&] { result += suffix; });
    } else {
        const int point = exponent + 1;
        if (point <= 0) allocateContractStorage([&] {
            result += "0." + std::string(static_cast<std::size_t>(-point), '0') + digits;
        });
        else if (static_cast<std::size_t>(point) >= digits.size())
            allocateContractStorage([&] {
                result += digits + std::string(static_cast<std::size_t>(point) - digits.size(), '0');
            });
        else allocateContractStorage([&] {
            digits.insert(static_cast<std::size_t>(point), 1, '.');
            result += digits;
        });
    }
    if (result.find_first_of("e.") == std::string::npos)
        allocateContractStorage([&] { result += ".0"; });
    return result;
}

} // namespace nijigenerate::autorig::deterministic::contracts::detail
#if defined(_MSC_VER) && defined(_M_X64)
#pragma float_control(pop)
#endif
