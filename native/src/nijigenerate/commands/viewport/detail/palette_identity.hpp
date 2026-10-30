#pragma once

#include <nijigenerate/commands/command_table.hpp>

namespace nijigenerate::commands::viewport::palette::detail {
namespace {

// The D alias argument names an actual command AA. Native callers pass that
// same current-thread map by reference, without copying or materializing it.
template<class Key>
bool inAA(const CommandTableStorage<Key>& commands, std::shared_ptr<Command> command) {
    for (const auto& entry : commands) {
        if (entry.second == command) return true;
    }
    return false;
}

} // namespace
} // namespace nijigenerate::commands::viewport::palette::detail
