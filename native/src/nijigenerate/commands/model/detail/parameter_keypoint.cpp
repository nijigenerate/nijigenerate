// Independent commands/model/set_deform_binding.d helper. BSD-2-Clause; see LICENSE.
#include "parameter_keypoint.hpp"
#include <cmath>
#include <cstdint>

namespace nijigenerate::commands::model::set_deform_binding::detail {

bool ngResolveParameterValueToKeyPoint(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        nicxlive::core::math::Vec2 value, compat::Vec2u& keyPoint, compat::RetainedStringSlice& message) {
    keyPoint = {};
    message = {};
    if (!parameter) {
        message = compat::RetainedString::fromStaticLiteral("context.parameterValue requires a parameter");
        return false;
    }

    // Keep the original uint local domain explicit, independently of native key
    // storage widths, before asking that same real provider for the key value.
    const auto x = static_cast<std::uint32_t>(
        parameter->getClosestAxisPointIndex(0, parameter->mapAxis(0, value.x)));
    const auto y = static_cast<std::uint32_t>(
        parameter->getClosestAxisPointIndex(1, parameter->mapAxis(1, value.y)));
    const auto resolved = parameter->getKeypointValue(nicxlive::core::param::Vec2u{x, y});
    constexpr float epsilon = 1e-5f;
    if (std::abs(resolved.x - value.x) > epsilon || std::abs(resolved.y - value.y) > epsilon) {
        message = compat::RetainedString::fromStaticLiteral(
            "context.parameterValue does not match an existing key value");
        return false;
    }

    keyPoint = {x, y};
    return true;
}

} // namespace nijigenerate::commands::model::set_deform_binding::detail
