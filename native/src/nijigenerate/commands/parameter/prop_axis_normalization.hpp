#pragma once

#include <nijigenerate/compat/phobos_float_sort.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands::parameter::prop::detail {

// Native internal identity for run's local Pair; no parameter/binding stand-in.
struct AxisRemapPair {
    std::size_t xi = 0;
    std::size_t yi = 0;
    std::size_t ox = 0;
    std::size_t oy = 0;
    float d = std::numeric_limits<float>::quiet_NaN();
};

// The original nested helper captures no command or model state.
inline bool normalizeAxis(std::uint32_t axis, compat::Slice<float> values,
        compat::Slice<float>& normalized, compat::RetainedStringSlice& axisMessage) {
    // D out arguments reset descriptors after the by-value input is captured.
    normalized = {};
    axisMessage = {};
    const auto setMessage = [&](const char* suffix) {
        try {
            axisMessage = compat::RetainedStringSlice{compat::RetainedString(
                std::string("axis") + (axis == 0 ? "X" : "Y") + suffix)};
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    };
    for (const float value : values) {
        if (!std::isfinite(value)) {
            setMessage(" contains non-finite value");
            return false;
        }
        if (value < 0 || value > 1) {
            setMessage(" values must be normalized between 0 and 1");
            return false;
        }
    }

    compat::phobos_float_sort::sort(std::span<float>{values.data(), values.size()});
    constexpr float eps = 1e-6f;
    normalized.append(0.0f);
    for (const float value : values) {
        if (std::abs(value) <= eps || std::abs(value - 1.0f) <= eps) continue;
        if (value <= 0 || value >= 1) continue;
        if (normalized.empty() || std::abs(normalized[normalized.size() - 1] - value) > eps)
            normalized.append(value);
    }
    normalized.append(1.0f);
    return true;
}

} // namespace nijigenerate::commands::parameter::prop::detail
