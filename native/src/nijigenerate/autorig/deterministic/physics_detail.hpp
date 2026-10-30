#pragma once
#include "controls_double_sort.hpp"
#include "physics.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nijigenerate::autorig::deterministic::physics_detail {
[[noreturn]] inline void allocationFailed() {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 519);
}

[[noreturn]] inline void duplicationFailed() {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
}

template<std::size_t N> compat::RetainedString duplicateEnforcementMessage(const char (&message)[N]) {
    static_assert(N > 1);
    try {
        return compat::RetainedString(std::string_view(message, N - 1));
    } catch (const std::bad_alloc&) {
        duplicationFailed();
    } catch (const std::length_error&) {
        duplicationFailed();
    }
}

template<std::size_t N> [[noreturn]] void failEnforcement(const char (&message)[N], std::size_t sourceLine,
        [[maybe_unused]] std::source_location source = std::source_location::current()) {
    auto retainedMessage = duplicateEnforcementMessage(message);
    throw compat::ThrowableException(std::move(retainedMessage),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"),
        sourceLine);
}

template<class T> compat::Slice<T> newArray(std::size_t count) {
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, double> || std::is_same_v<T, std::uint64_t> ||
        std::is_same_v<T, contracts::Point2> || std::is_same_v<T, compat::Slice<std::size_t>>);
    if (!count) return {};
    try {
        if constexpr (std::is_same_v<T, double>)
            return compat::Slice<T>(std::vector<T>(count, std::numeric_limits<double>::quiet_NaN()));
        else
            return compat::Slice<T>(std::vector<T>(count));
    } catch (const std::bad_alloc&) {
        allocationFailed();
    } catch (const std::length_error&) {
        allocationFailed();
    }
}

inline compat::Slice<contracts::Point2> duplicatePoints(compat::Slice<contracts::Point2> points) {
    try {
        return points.duplicate();
    } catch (const std::bad_alloc&) {
        duplicationFailed();
    } catch (const std::length_error&) {
        duplicationFailed();
    }
}

inline double smooth(double value) {
    double s = value;
    if (s < 0.) s = 0.;
    else if (1. < s) s = 1.;
    return s * s * (3 - 2 * s);
}
using PercentileInputError = compat::ThrowableException;
inline double percentile(std::span<const double> input, double fraction) {
    if (input.empty()) failEnforcement("Empty physics support", 47);
    auto sorted = [&] {
        try {
            return std::vector<double>(input.begin(), input.end());
        } catch (const std::bad_alloc&) {
            duplicationFailed();
        } catch (const std::length_error&) {
            duplicationFailed();
        }
    }();
    controls_double_sort::sort(sorted);
    auto values = [&] {
        try {
            return std::vector<double>(sorted.begin(), sorted.end());
        } catch (const std::bad_alloc&) {
            allocationFailed();
        } catch (const std::length_error&) {
            allocationFailed();
        }
    }();
    const double position = fraction * (values.size() - 1);
    const auto i = static_cast<std::size_t>(position);
    const auto read = [&values](std::size_t index) {
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"), 49);
        return values[index];
    };
    const auto left = read(i) * (1 - (position - i));
    return left + read(std::min(i + 1, values.size() - 1)) * (position - i);
}
}
