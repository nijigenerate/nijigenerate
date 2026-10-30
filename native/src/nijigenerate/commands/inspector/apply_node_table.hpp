#pragma once
#include "apply_node.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::inspector::apply_node {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<InspectorNodeApplyCommand>;
extern thread_local CommandTable commands;
}
