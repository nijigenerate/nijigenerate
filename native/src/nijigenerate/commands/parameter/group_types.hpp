#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::parameter::group {
enum class GroupCommand : std::int32_t {
    MoveParameter = 0,
    CreateParamGroup = 1,
    ChangeGroupColor = 2,
    DeleteParamGroup = 3
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::parameter::group::GroupCommand> {
    using Enum = commands::parameter::group::GroupCommand;
    inline static constexpr std::string_view name = "GroupCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::MoveParameter, "MoveParameter"},
        FormatEnumMember<Enum>{Enum::CreateParamGroup, "CreateParamGroup"},
        FormatEnumMember<Enum>{Enum::ChangeGroupColor, "ChangeGroupColor"},
        FormatEnumMember<Enum>{Enum::DeleteParamGroup, "DeleteParamGroup"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::group::GroupCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.group.GroupCommand)";
    inline static constexpr char unqualifiedArgument[] = "(GroupCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.group.GroupCommand";
    inline static constexpr char unqualifiedElement[] = "GroupCommand";
};
} // namespace nicxlive::core::d_runtime
