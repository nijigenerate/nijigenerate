#pragma once
#include "command_table.hpp"
#include "command_scope.hpp"
#include "parameter/animedit.hpp"
#include "puppet/view.hpp"
#include "node/dynamic_keys.hpp"

namespace nijigenerate::commands {
namespace parameter::animedit {
inline thread_local CommandTableStorage<AnimeditCommand> commands;
}
namespace puppet::view {
inline thread_local CommandTableStorage<ViewCommand> commands;
}
namespace node::node {
template<bool> class AddNodeCommandT;
template<bool> class InsertNodeCommandT;
}
namespace node::dynamic {
template<bool> class ConvertNodeToCommand;
struct NodeCommandKeyHash {
    template<class K> std::size_t operator()(const K& key) const { return key.toHash(); }
};
template<class K> struct DynamicCommandValue;
template<> struct DynamicCommandValue<AddNodeKey> { using Type = node::AddNodeCommandT<false>; };
template<> struct DynamicCommandValue<InsertNodeKey> { using Type = node::InsertNodeCommandT<false>; };
template<> struct DynamicCommandValue<ConvertToKey> { using Type = ConvertNodeToCommand<false>; };
template<class K> using DynamicCommandTable = CommandMapStorage<K,
    std::shared_ptr<typename DynamicCommandValue<K>::Type>, NodeCommandKeyHash>;
inline thread_local DynamicCommandTable<AddNodeKey> addNodeCommands;
inline thread_local DynamicCommandTable<InsertNodeKey> insertNodeCommands;
inline thread_local DynamicCommandTable<ConvertToKey> convertNodeCommands;
}
}
