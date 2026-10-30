#pragma once
#include "edit_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::puppet::edit {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<EditCommand>;
extern thread_local CommandTable commands;
}
