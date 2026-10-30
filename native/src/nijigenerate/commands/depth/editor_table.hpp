#pragma once
#include "editor_types.hpp"
#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::depth::editor {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<DepthEditorOperationCommand>;
extern thread_local CommandTable commands;
}
