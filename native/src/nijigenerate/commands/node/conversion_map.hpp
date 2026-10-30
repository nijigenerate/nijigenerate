#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include <common/string_key_map.hpp>
#include "nijigenerate/compat/slice.hpp"
#include <array>

namespace nijigenerate::commands::node::base {
using ConversionValues = compat::Slice<compat::RetainedString>;
using ConversionMap = ::nicxlive::core::common::StringKeyMap<ConversionValues>;
inline thread_local ConversionMap _conversionMap;

namespace detail {
inline const std::array<compat::RetainedString, 6>& conversionLiteralStrings() {
    static const std::array strings{
        compat::RetainedString::fromStaticLiteral("Node"),
        compat::RetainedString::fromStaticLiteral("MeshGroup"),
        compat::RetainedString::fromStaticLiteral("DynamicComposite"),
        compat::RetainedString::fromStaticLiteral("GridDeformer"),
        compat::RetainedString::fromStaticLiteral("Part"),
        compat::RetainedString::fromStaticLiteral("Composite")};
    return strings;
}
inline std::array<ConversionValues, 5>& conversionLiteralValues() {
    // Selected LDC 1.41 constant AA lowering retains mutable value-array globals.
    const auto& strings = conversionLiteralStrings();
    static std::array nodeValues{strings[1], strings[2], strings[3]};
    static std::array dynamicValues{strings[1], strings[0], strings[4], strings[5]};
    static std::array meshGroupValues{strings[2], strings[0], strings[3]};
    static std::array compositeValues{strings[2], strings[0]};
    static std::array gridValues{strings[1], strings[0]};
    static std::array<ConversionValues, 5> values{{
        ConversionValues::fromStaticBuffer(nodeValues.data(), nodeValues.size()),
        ConversionValues::fromStaticBuffer(dynamicValues.data(), dynamicValues.size()),
        ConversionValues::fromStaticBuffer(meshGroupValues.data(), meshGroupValues.size()),
        ConversionValues::fromStaticBuffer(compositeValues.data(), compositeValues.size()),
        ConversionValues::fromStaticBuffer(gridValues.data(), gridValues.size())
    }};
    return values;
}
}

inline ConversionMap conversionMap() {
    if (_conversionMap.empty()) {
        auto& values = detail::conversionLiteralValues();
        const auto& strings = detail::conversionLiteralStrings();
        ConversionMap initialized;
        initialized.setRetained(strings[0], values[0]);
        initialized.setRetained(strings[2], values[1]);
        initialized.setRetained(strings[1], values[2]);
        initialized.setRetained(strings[5], values[3]);
        initialized.setRetained(strings[3], values[4]);
        _conversionMap = initialized;
    }
    return _conversionMap;
}
}
