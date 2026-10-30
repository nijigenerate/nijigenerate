#pragma once
#include "node_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::node::node {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<NodeCommand>;
extern thread_local CommandTable commands;
}
