#pragma once
#include "param_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::parameter::param {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<ParamCommand>;
extern thread_local CommandTable commands;
}
