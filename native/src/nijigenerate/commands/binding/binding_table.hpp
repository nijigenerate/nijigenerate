#pragma once
#include "binding_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::binding {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<BindingCommand>;
extern thread_local CommandTable commands;
}
