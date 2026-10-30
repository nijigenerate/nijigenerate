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

namespace nijigenerate::commands::node::welding {

enum class NodeWeldingCommand : std::int32_t {
    AddWelding = 0,
    RemoveWelding = 1,
    ChangeWeldingWeight = 2
};

class AddWeldingCommand;
class RemoveWeldingCommand;
class ChangeWeldingWeightCommand;

using CommandTable = ::nijigenerate::commands::CommandTableStorage<NodeWeldingCommand>;
extern thread_local CommandTable commands;

void ngInitWeldingCommands();

}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::commands::node::welding::NodeWeldingCommand> {
    using Enum = ::nijigenerate::commands::node::welding::NodeWeldingCommand;
    inline static constexpr std::string_view name = "NodeWeldingCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::AddWelding, "AddWelding"},
        FormatEnumMember<Enum>{Enum::RemoveWelding, "RemoveWelding"},
        FormatEnumMember<Enum>{Enum::ChangeWeldingWeight, "ChangeWeldingWeight"}};
};
}
namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::node::welding::NodeWeldingCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.node.welding.NodeWeldingCommand)";
    inline static constexpr char unqualifiedArgument[] = "(NodeWeldingCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.node.welding.NodeWeldingCommand";
    inline static constexpr char unqualifiedElement[] = "NodeWeldingCommand";
};
}

#include "welding_commands.hpp"
