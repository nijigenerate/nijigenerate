#pragma once
#include "standard_depth_types.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <math/types.hpp>
#include <limits>

namespace nijigenerate::commands::depth::bone {
namespace {

struct StandardDepthParameterSpec {
    compat::RetainedString name;
    bool isVec2 = false;
    nicxlive::core::math::Vec2 minValue{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    nicxlive::core::math::Vec2 maxValue{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    compat::Slice<float> axisX;
    compat::Slice<float> axisY;
};

struct StandardDepthBindingValue {
    nicxlive::core::math::Vec2 paramValue{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    float value = std::numeric_limits<float>::quiet_NaN();
};

struct StandardDepthBoneBindingSpec {
    compat::RetainedString parameterName;
    compat::RetainedString boneId;
    compat::RetainedString bindingName;
    compat::Slice<StandardDepthBindingValue> values;
};

} // namespace
} // namespace nijigenerate::commands::depth::bone
