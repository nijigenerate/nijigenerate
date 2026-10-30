#pragma once
#include <cstdint>
#include <concepts>
#include <memory>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands { class Command; }

namespace nijigenerate::commands::node::mask {

enum class NodeMaskCommand : std::int32_t {
    AddMask = 0,
    RemoveMask = 1,
    ChangeMaskMode = 2
};

class AddMaskCommand;
class RemoveMaskCommand;
class ChangeMaskModeCommand;

using CommandTable = ::nijigenerate::commands::CommandTableStorage<NodeMaskCommand>;
extern thread_local CommandTable commands;

void ngInitNodeMaskCommands();

}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::commands::node::mask::NodeMaskCommand> {
    using Enum = ::nijigenerate::commands::node::mask::NodeMaskCommand;
    inline static constexpr std::string_view name = "NodeMaskCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::AddMask, "AddMask"},
        FormatEnumMember<Enum>{Enum::RemoveMask, "RemoveMask"},
        FormatEnumMember<Enum>{Enum::ChangeMaskMode, "ChangeMaskMode"}};
};
}
namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::node::mask::NodeMaskCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.node.mask.NodeMaskCommand)";
    inline static constexpr char unqualifiedArgument[] = "(NodeMaskCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.node.mask.NodeMaskCommand";
    inline static constexpr char unqualifiedElement[] = "NodeMaskCommand";
};
}

#include "mask_commands.hpp"
