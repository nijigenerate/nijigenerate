#pragma once
#include "paramedit_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::parameter::paramedit {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<ParameditCommand>;
extern thread_local CommandTable commands;
}
