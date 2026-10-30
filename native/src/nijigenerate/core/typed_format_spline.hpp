#pragma once
#include "typed_format.hpp"
#include "typed_format_vec2.hpp"
#include <nijigenerate/viewport/common/spline.hpp>
#include <cstddef>
#include <tuple>

namespace nijigenerate::core::translation {
// Actual declaration-side producer for spline.d42-46. The general aggregate
// formatter is open to independently supplied genuine field metadata; this
// producer does not claim reflection for unlisted or unrelated Source types.
template<> struct FormatAggregateTraits<viewport::common::spline::SplinePoint> {
    using Value = viewport::common::spline::SplinePoint;
    inline static constexpr std::string_view typeName = "SplinePoint";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("position", offsetof(Value, position), &Value::position),
            ngFormatAggregateMember("weightL", offsetof(Value, weightL), &Value::weightL),
            ngFormatAggregateMember("weightR", offsetof(Value, weightR), &Value::weightR)};
    }
};
} // namespace nijigenerate::core::translation
