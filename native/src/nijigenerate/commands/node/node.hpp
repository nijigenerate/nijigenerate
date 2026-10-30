#pragma once
#include "node_types.hpp"
#include "node_table.hpp"
#include <concepts>

namespace nijigenerate::commands::node::node {

template<bool exposeClassName = true> class AddNodeCommandT;
template<bool exposeClassName = true> class InsertNodeCommandT;
template<bool exposeClassName = true> class ConvertToCommandT;
using AddNodeCommand = AddNodeCommandT<true>;
using InsertNodeCommand = InsertNodeCommandT<true>;
using ConvertToCommand = ConvertToCommandT<true>;
class MoveNodeCommand;
class SetNodeNameCommand;
class DeleteNodeCommand;
class CutNodeCommand;
class CopyNodeCommand;
class PasteNodeCommand;
class ReloadNodeCommand;
class VertexModeCommand;
class ToggleVisibilityCommand;
class CentralizeNodeCommand;


}

#include "node_commands.hpp"
