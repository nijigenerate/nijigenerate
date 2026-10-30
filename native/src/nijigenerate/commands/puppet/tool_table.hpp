#pragma once
#include "tool_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::puppet::tool {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<ToolCommand>;
extern thread_local CommandTable commands;
}
