#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::parameter::param {
enum class ParamCommand : std::int32_t {
    Add1DParameter = 0,
    Add2DParameter = 1,
    AddMouthParameter = 2,
    RemoveParameter = 3
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::parameter::param::ParamCommand> {
    using Enum = commands::parameter::param::ParamCommand;
    inline static constexpr std::string_view name = "ParamCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Add1DParameter, "Add1DParameter"},
        FormatEnumMember<Enum>{Enum::Add2DParameter, "Add2DParameter"},
        FormatEnumMember<Enum>{Enum::AddMouthParameter, "AddMouthParameter"},
        FormatEnumMember<Enum>{Enum::RemoveParameter, "RemoveParameter"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::param::ParamCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.param.ParamCommand)";
    inline static constexpr char unqualifiedArgument[] = "(ParamCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.param.ParamCommand";
    inline static constexpr char unqualifiedElement[] = "ParamCommand";
};
} // namespace nicxlive::core::d_runtime
