#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::parameter::prop {
enum class ParamPropCommand : std::int32_t {
    SetParameterName = 0,
    ApplyParameterPropsAxes = 1
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::parameter::prop::ParamPropCommand> {
    using Enum = commands::parameter::prop::ParamPropCommand;
    inline static constexpr std::string_view name = "ParamPropCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::SetParameterName, "SetParameterName"},
        FormatEnumMember<Enum>{Enum::ApplyParameterPropsAxes, "ApplyParameterPropsAxes"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::prop::ParamPropCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.prop.ParamPropCommand)";
    inline static constexpr char unqualifiedArgument[] = "(ParamPropCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.prop.ParamPropCommand";
    inline static constexpr char unqualifiedElement[] = "ParamPropCommand";
};
} // namespace nicxlive::core::d_runtime
