#pragma once
#include "palette_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::viewport::palette {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<PaletteCommand>;
extern thread_local CommandTable commands;
}
