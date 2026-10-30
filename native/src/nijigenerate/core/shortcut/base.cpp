#include "base_internal.hpp"
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::core::shortcut {
namespace detail {
thread_local ShortcutRegistry shortcutEntries;
}
namespace {
using CommandRef = std::shared_ptr<commands::Command>;
using detail::shortcutEntries;
template<class Operation> decltype(auto) allocateShortcutArray(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

void ngRegisterShortcut(compat::RetainedString shortcut, CommandRef command, bool repeat) {
    if (!command || shortcut.empty()) return;
    const auto keys = allocateShortcutArray([&] { return shortcutEntries.keys(); });
    for (const auto& existing : keys) {
        const auto value = shortcutEntries.findRetained(existing);
        if (!value) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/shortcut/base.d"), 47);
        const auto entry = *value;
        if (entry.shortcut == shortcut) {
            shortcutEntries.erase(existing);
            break;
        }
    }
    ngClearShortcutFor(command);
    shortcutEntries[command] = ActionEntry{shortcut, command, repeat};
}

void ngClearShortcutFor(CommandRef command) {
    if (!command) return;
    shortcutEntries.erase(command);
}

compat::Slice<ActionEntry> ngListShortcuts() {
    auto values = allocateShortcutArray([&] { return shortcutEntries.values(); });
    return compat::Slice<ActionEntry>::retainedView(values.retentionOwner(), values.data(), values.size());
}

compat::RetainedString ngShortcutFor(CommandRef command) {
    const auto found = shortcutEntries.findRetained(command);
    if (found) return found->shortcut;
    return compat::RetainedString::emptyLiteral();
}

} // namespace nijigenerate::core::shortcut
