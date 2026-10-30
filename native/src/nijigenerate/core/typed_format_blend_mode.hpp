#pragma once
#include "typed_format_enum.hpp"
#include <nodes/common.hpp>
#include <array>

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<nicxlive::core::nodes::BlendMode> {
    using Value=nicxlive::core::nodes::BlendMode;
    inline static constexpr std::string_view name="BlendMode";
    // Original declaration order; the native-only Additive alias is excluded.
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Normal,"Normal"},
        FormatEnumMember<Value>{Value::Multiply,"Multiply"},
        FormatEnumMember<Value>{Value::Screen,"Screen"},
        FormatEnumMember<Value>{Value::Overlay,"Overlay"},
        FormatEnumMember<Value>{Value::Darken,"Darken"},
        FormatEnumMember<Value>{Value::Lighten,"Lighten"},
        FormatEnumMember<Value>{Value::ColorDodge,"ColorDodge"},
        FormatEnumMember<Value>{Value::LinearDodge,"LinearDodge"},
        FormatEnumMember<Value>{Value::AddGlow,"AddGlow"},
        FormatEnumMember<Value>{Value::ColorBurn,"ColorBurn"},
        FormatEnumMember<Value>{Value::HardLight,"HardLight"},
        FormatEnumMember<Value>{Value::SoftLight,"SoftLight"},
        FormatEnumMember<Value>{Value::Difference,"Difference"},
        FormatEnumMember<Value>{Value::Exclusion,"Exclusion"},
        FormatEnumMember<Value>{Value::Subtract,"Subtract"},
        FormatEnumMember<Value>{Value::Inverse,"Inverse"},
        FormatEnumMember<Value>{Value::DestinationIn,"DestinationIn"},
        FormatEnumMember<Value>{Value::ClipToLower,"ClipToLower"},
        FormatEnumMember<Value>{Value::SliceFromLower,"SliceFromLower"}};
};
}
