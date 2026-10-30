#pragma once
#include "group_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::parameter::group {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<GroupCommand>;
extern thread_local CommandTable commands;
}
