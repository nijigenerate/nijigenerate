#pragma once

#include <nijigenerate/compat/vec2u.hpp>
#include <param/parameter.hpp>
#include <cmath>
#include <memory>
#include <optional>
#include <string_view>

namespace nijigenerate::commands::binding::detail {
// This private helper only returns null or one of two static string literals.
inline bool ngResolveBindingParameterValueToKeyPoint(
    std::shared_ptr<::nicxlive::core::param::Parameter> param,
    ::nicxlive::core::math::Vec2 value, compat::Vec2u& keyPoint,
    std::optional<std::string_view>& message) {
    keyPoint = {};
    message.reset();
    if (!param) {
        message = "context.parameterValue requires a parameter";
        return false;
    }
    const std::uint32_t x = param->getClosestAxisPointIndex(0, param->mapAxis(0, value.x));
    const std::uint32_t y = param->getClosestAxisPointIndex(1, param->mapAxis(1, value.y));
    const auto resolved = param->getKeypointValue({x, y});
    constexpr float epsilon = 1e-5f;
    if (std::abs(resolved.x - value.x) > epsilon || std::abs(resolved.y - value.y) > epsilon) {
        message = "context.parameterValue does not match an existing key value";
        return false;
    }
    keyPoint = {x, y};
    return true;
}
} // namespace nijigenerate::commands::binding::detail
