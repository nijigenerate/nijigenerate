#pragma once

#include <nijigenerate/core/settings_value.hpp>
#include <math/types.hpp>
#include <limits>

namespace nijigenerate::api::mcp::helpers::detail {
inline bool _jsonNumber(core::settings::SettingsJsonValue value, float& result) {
    // D out float parameters are initialized before executing the body.
    result = std::numeric_limits<float>::quiet_NaN();
    if (value.is_double()) {
        result = static_cast<float>(value.as_double());
        return true;
    }
    if (value.is_int64()) {
        result = static_cast<float>(static_cast<double>(value.as_int64()));
        return true;
    }
    return false;
}

inline bool _readParamValue(core::settings::SettingsJsonValue value,
    nicxlive::core::math::Vec2& parameterValue, compat::RetainedStringSlice& message) {
    // inmath Vector!(float,2).init initializes both stored float elements to NaN.
    parameterValue = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    message.reset();
    if (!value.is_array() || value.as_array().empty()) {
        message = compat::RetainedString::fromStaticLiteral("context.parameterValue must be [x] or [x,y]");
        return false;
    }
    float x;
    if (!_jsonNumber(value.as_array()[0], x)) {
        message = compat::RetainedString::fromStaticLiteral("context.parameterValue[0] must be a number");
        return false;
    }
    float y = 0;
    if (value.as_array().size() >= 2 && !_jsonNumber(value.as_array()[1], y)) {
        message = compat::RetainedString::fromStaticLiteral("context.parameterValue[1] must be a number");
        return false;
    }
    parameterValue = {x, y};
    return true;
}
} // namespace nijigenerate::api::mcp::helpers::detail
