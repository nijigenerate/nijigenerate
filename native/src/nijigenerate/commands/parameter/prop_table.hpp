#pragma once
#include "prop_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::parameter::prop {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<ParamPropCommand>;
extern thread_local CommandTable commands;
}
