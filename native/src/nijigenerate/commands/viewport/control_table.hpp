#pragma once
#include "control_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::viewport::control {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<ViewportCommand>;
extern thread_local CommandTable commands;
}
