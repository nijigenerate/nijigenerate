#pragma once

#include <nijigenerate/commands/detail/map_validation.hpp>
#include <nijigenerate/commands/puppet/tool_table.hpp>
#include <array>

namespace nijigenerate::commands::detail {

template<> struct CommandMapKeyMetadata<puppet::tool::ToolCommand> {
    using Key = puppet::tool::ToolCommand;
    using Member = CommandEnumMember<Key>;
    inline static constexpr std::string_view typeName = "ToolCommand";
    inline static constexpr std::array members{
        Member{Key::ShowImportSessionDataDialog, "ShowImportSessionDataDialog"},
        Member{Key::ImportSessionData, "ImportSessionData"},
        Member{Key::PremultTexture, "PremultTexture"},
        Member{Key::RebleedTexture, "RebleedTexture"},
        Member{Key::RegenerateMipmaps, "RegenerateMipmaps"},
        Member{Key::GenerateFakeLayerName, "GenerateFakeLayerName"},
        Member{Key::AttemptRepairPuppet, "AttemptRepairPuppet"},
        Member{Key::RegenerateNodeIDs, "RegenerateNodeIDs"},
        Member{Key::ModelEditMode, "ModelEditMode"},
        Member{Key::AnimEditMode, "AnimEditMode"},
        Member{Key::ExecuteAutoRig, "ExecuteAutoRig"},
        Member{Key::GetAutoRigStatus, "GetAutoRigStatus"},
        Member{Key::GetAutoRigMemoryStatus, "GetAutoRigMemoryStatus"},
        Member{Key::DeleteAutoRigSession, "DeleteAutoRigSession"},
        Member{Key::ResumeAutoRig, "ResumeAutoRig"},
    };
};

// This names the sole existing TLS map. It is not a second registry and does
// not take the TLS object's address as a compile-time template argument.
struct ToolCommandMapBinding {
    using map_type = puppet::tool::CommandTable;
    static map_type& get() { return puppet::tool::commands; }
};

} // namespace nijigenerate::commands::detail
