#pragma once

#include <nijigenerate/core/settings_value.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace nijigenerate::autorig::deterministic::editor::detail {
inline std::uint32_t identifier(core::settings::SettingsJsonValue value) {
    if (!(value.is_uint64() || (value.is_int64() && value.as_int64() >= 0)))
        throw std::runtime_error("Expected an unsigned model identifier");
    const auto number = value.is_uint64() ? value.as_uint64() : static_cast<std::uint64_t>(value.as_int64());
    if (number > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Model identifier out of range");
    return static_cast<std::uint32_t>(number);
}

inline double coordinate(core::settings::SettingsJsonValue value) {
    if (!(value.is_double() || value.is_int64() || value.is_uint64()))
        throw std::runtime_error("Expected a deformation coordinate");
    const double result = value.is_double() ? value.as_double() : value.is_int64()
        ? static_cast<double>(value.as_int64()) : static_cast<double>(value.as_uint64());
    if (!std::isfinite(result)) throw std::runtime_error("Nonfinite deformation coordinate");
    return result;
}
} // namespace nijigenerate::autorig::deterministic::editor::detail
