#pragma once

#include <cstdint>
#include <array>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::common::mesheditor::tools::enums {

enum class VertexToolMode : std::int32_t {
    Points = 0,
    Connect = 1,
    PathDeform = 2,
    Grid = 3,
    Brush = 4,
    BezierDeform = 5,
    AltGridDeform = 6,
    AltBezierDeform = 7,
    LassoSelection = 8,
    EdgeCutter = 9,
};

} // namespace nijigenerate::viewport::common::mesheditor::tools::enums

namespace nijigenerate::core::translation {

template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode;
    inline static constexpr std::string_view name = "VertexToolMode";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Points, "Points"},
        FormatEnumMember<Enum>{Enum::Connect, "Connect"},
        FormatEnumMember<Enum>{Enum::PathDeform, "PathDeform"},
        FormatEnumMember<Enum>{Enum::Grid, "Grid"},
        FormatEnumMember<Enum>{Enum::Brush, "Brush"},
        FormatEnumMember<Enum>{Enum::BezierDeform, "BezierDeform"},
        FormatEnumMember<Enum>{Enum::AltGridDeform, "AltGridDeform"},
        FormatEnumMember<Enum>{Enum::AltBezierDeform, "AltBezierDeform"},
        FormatEnumMember<Enum>{Enum::LassoSelection, "LassoSelection"},
        FormatEnumMember<Enum>{Enum::EdgeCutter, "EdgeCutter"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral(
                    "(nijigenerate.viewport.common.mesheditor.tools.enums.VertexToolMode)"),
            RetainedString::fromStaticLiteral("(VertexToolMode)"),
            RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.mesheditor.tools.enums.VertexToolMode"),
            RetainedString::fromStaticLiteral("VertexToolMode")};
    }
};
} // namespace nicxlive::core::d_runtime
