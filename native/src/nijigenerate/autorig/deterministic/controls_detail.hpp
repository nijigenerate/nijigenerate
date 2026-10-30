#pragma once
#include "controls_double_sort.hpp"
#include "contracts.hpp"
#include <compat/object_metadata.hpp>
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <new>
#include <source_location>
#include <span>
#include <type_traits>
#include <vector>

namespace nijigenerate::autorig::deterministic::controls_detail {
template<class Key, class Value, class Hash, class Equal>
Value readControlMap(const nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal>& values,
        const std::type_identity_t<Key>& key, std::size_t line) {
    const auto found = values.find(key);
    if (found == values.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
    return found->second;
}
struct Profile {
    compat::Slice<double> axis, upper, lower;
    double sample(std::span<const double> values, double x) {
        const auto read = [](std::span<const double> values, std::size_t i, std::size_t line) {
            if (i >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, values.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
            return values[i];
        };
        const auto stations = std::span<const double>(axis.data(), axis.size());
        if (x <= read(stations, 0, 72)) return read(values, 0, 72);
        for (std::size_t i = 1; i < axis.size(); ++i) {
            if (x <= read(stations, i, 73)) {
                const double t = (x - read(stations, i - 1, 74)) /
                    (read(stations, i, 74) - read(stations, i - 1, 74));
                const double lower = read(values, i - 1, 74) * (1 - t);
                const double upper = read(values, i, 74) * t;
                return lower + upper;
            }
        }
        return read(values, values.size() - 1, 76);
    }
};
inline double curveSample(std::span<const double> axis, std::span<const double> values, double x) {
    if (axis.size() != values.size() || axis.empty())
        throw compat::ThrowableException(
            contracts::detail::copyContractMessage("Invalid measured section curve"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), 493);
    if (x <= axis[0]) return values[0];
    for (std::size_t i = 1; i < axis.size(); ++i) {
        if (x <= axis[i]) {
            const double t = (x - axis[i - 1]) / (axis[i] - axis[i - 1]);
            return values[i - 1] * (1 - t) + values[i] * t;
        }
    }
    return values.back();
}
inline double smoothStep(double x) {
    if (x < 0.) x = 0.;
    else if (1. < x) x = 1.;
    return x * x * (3 - 2 * x);
}
inline double percentile(std::span<const double> input, double fraction) {
    if (input.empty()) throw compat::ThrowableException(
        contracts::detail::copyContractMessage("Empty facial support"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), 20);
    auto values = [&] {
        auto sorted = [&] {
            try {
                return std::vector<double>(input.begin(), input.end());
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            }
        }();
        controls_double_sort::sort(sorted);
        try {
            return std::vector<double>(sorted.begin(), sorted.end());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    const double index = fraction * (values.size() - 1);
    const auto i = static_cast<std::size_t>(index);
    if (i >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), 22);
    return values[i] * (1 - (index - i)) + values[std::min(i + 1, values.size() - 1)] * (index - i);
}

} // namespace nijigenerate::autorig::deterministic::controls_detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::controls_detail::Profile> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.controls.Profile)";
    inline static constexpr char unqualifiedArgument[] = "(Profile)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.controls.Profile";
    inline static constexpr char unqualifiedElement[] = "Profile";
};
}
