#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::puppet::tool {
enum class ToolCommand : std::int32_t {
    ShowImportSessionDataDialog = 0,
    ImportSessionData = 1,
    PremultTexture = 2,
    RebleedTexture = 3,
    RegenerateMipmaps = 4,
    GenerateFakeLayerName = 5,
    AttemptRepairPuppet = 6,
    RegenerateNodeIDs = 7,
    ModelEditMode = 8,
    AnimEditMode = 9,
    ExecuteAutoRig = 10,
    GetAutoRigStatus = 11,
    GetAutoRigMemoryStatus = 12,
    DeleteAutoRigSession = 13,
    ResumeAutoRig = 14
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::puppet::tool::ToolCommand> {
    using Enum = commands::puppet::tool::ToolCommand;
    inline static constexpr std::string_view name = "ToolCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::ShowImportSessionDataDialog, "ShowImportSessionDataDialog"},
        FormatEnumMember<Enum>{Enum::ImportSessionData, "ImportSessionData"},
        FormatEnumMember<Enum>{Enum::PremultTexture, "PremultTexture"},
        FormatEnumMember<Enum>{Enum::RebleedTexture, "RebleedTexture"},
        FormatEnumMember<Enum>{Enum::RegenerateMipmaps, "RegenerateMipmaps"},
        FormatEnumMember<Enum>{Enum::GenerateFakeLayerName, "GenerateFakeLayerName"},
        FormatEnumMember<Enum>{Enum::AttemptRepairPuppet, "AttemptRepairPuppet"},
        FormatEnumMember<Enum>{Enum::RegenerateNodeIDs, "RegenerateNodeIDs"},
        FormatEnumMember<Enum>{Enum::ModelEditMode, "ModelEditMode"},
        FormatEnumMember<Enum>{Enum::AnimEditMode, "AnimEditMode"},
        FormatEnumMember<Enum>{Enum::ExecuteAutoRig, "ExecuteAutoRig"},
        FormatEnumMember<Enum>{Enum::GetAutoRigStatus, "GetAutoRigStatus"},
        FormatEnumMember<Enum>{Enum::GetAutoRigMemoryStatus, "GetAutoRigMemoryStatus"},
        FormatEnumMember<Enum>{Enum::DeleteAutoRigSession, "DeleteAutoRigSession"},
        FormatEnumMember<Enum>{Enum::ResumeAutoRig, "ResumeAutoRig"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::tool::ToolCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.tool.ToolCommand)";
    inline static constexpr char unqualifiedArgument[] = "(ToolCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.tool.ToolCommand";
    inline static constexpr char unqualifiedElement[] = "ToolCommand";
};
} // namespace nicxlive::core::d_runtime
