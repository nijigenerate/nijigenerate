#pragma once

#include <cstdint>
#include <compat/object_metadata.hpp>
#include "command_table.hpp"

namespace nijigenerate::commands {

enum class CommandScopeCommand : std::int32_t {
    GetCurrentCommandScope = 0,
};

inline thread_local CommandTableStorage<CommandScopeCommand> scopeCommands;

class GetCurrentCommandScopeCommand;
void ngInitCommandScopeCommands();

} // namespace nijigenerate::commands

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::CommandScopeCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.command_scope.CommandScopeCommand)";
    inline static constexpr char unqualifiedArgument[] = "(CommandScopeCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.command_scope.CommandScopeCommand";
    inline static constexpr char unqualifiedElement[] = "CommandScopeCommand";
};
}

#include "command_scope_command.hpp"
