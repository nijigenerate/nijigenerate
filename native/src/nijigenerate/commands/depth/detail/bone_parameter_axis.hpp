#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include <core/param/parameter.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace nijigenerate::commands::depth::bone {
namespace {

float parameterAxisValue(std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::size_t axis, std::size_t index) {
    compat::ngRequireObject(param.get());
    const float minValue = axis == 0 ? param->min.x : param->min.y;
    const float maxValue = axis == 0 ? param->max.x : param->max.y;
    const float span = maxValue - minValue;
    if (std::abs(span) <= 0.000001f) return minValue;
    return minValue + span * compat::ngArrayAt(compat::ngArrayAt(param->axisPoints, axis), index);
}

bool parameterHasAxisValue(std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::uint32_t axis, float value) {
    constexpr float epsilon = 0.000001f;
    compat::ngRequireObject(param.get());
    if (!param->isVec2 && axis == 1) return std::abs(value) <= epsilon;
    const auto count = compat::ngArrayAt(param->axisPoints, axis).size();
    for (std::size_t i = 0; i < count; ++i) {
        if (std::abs(parameterAxisValue(param, axis, i) - value) <= epsilon) return true;
    }
    return false;
}

} // namespace
} // namespace nijigenerate::commands::depth::bone
