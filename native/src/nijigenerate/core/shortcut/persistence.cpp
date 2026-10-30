#include "persistence.hpp"
#include "base_internal.hpp"
#include <nijigenerate/commands/all_maps.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
namespace nijigenerate::core::shortcut {
namespace {
using CommandRef = std::shared_ptr<commands::Command>;
template<class Operation> decltype(auto) allocateBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
auto commandIdMap() {
    detail::ShortcutIdMap result;
    commands::ngForEachCommandMap([&](auto& table) {
        const auto entries = table;
        for (auto [key, command] : entries) {
            auto idBytes = commands::ngCommandIdFromKey(key);
            const auto id = allocateBytes([&] { return compat::RetainedString(std::move(idBytes)); });
            result[id] = command;
            const auto typeName = commands::ngCommandKeyTypeName<std::remove_cvref_t<decltype(key)>>();
            const auto legacyPrefix = allocateBytes([&] { return std::string(typeName) + "."; });
            const auto legacyName = commands::ngCommandKeyName(key);
            const auto legacy = allocateBytes([&] { return compat::RetainedString(legacyPrefix + legacyName); });
            if (legacy != id) result[legacy] = command;
        }
    });
    return result;
}
compat::RetainedString commandIdFor(CommandRef command) {
    auto all = commandIdMap();
    compat::RetainedString best;
    for (auto [id, current] : all) {
        if (!nicxlive::core::common::ngClassObjectIs(current, command)) continue;
        if (best.empty()) best = id;
        const auto bytes = id.view();
        // Preserve the source's eight-byte AutoMesh prefix comparison against
        // the nine-byte literal rather than silently repairing its behavior.
        if ((bytes.size() >= 5 && bytes.substr(0, 5) == "Node.") ||
                (bytes.size() >= 6 && bytes.substr(0, 6) == "Panel.") ||
                (bytes.size() >= 8 && bytes.substr(0, 8) == "AutoMesh.")) {
            best = id;
            break;
        }
        if (best.view().find("Key.") != std::string_view::npos &&
                bytes.find("Key.") == std::string_view::npos) best = id;
    }
    return best;
}
}
void ngSaveShortcutsToSettings() {
    settings::SettingsStringMap object;
    const auto entries = detail::shortcutEntries;
    for (auto [command, entry] : entries) {
        (void)command;
        auto id = commandIdFor(entry.command);
        if (!id.empty()) object[id] = settings::SettingsString(entry.shortcut);
    }
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("Shortcuts"), object);
}
void ngLoadShortcutsFromSettings() {
    if (!settings::incSettingsCanGet(compat::RetainedString::fromStaticLiteral("Shortcuts"))) return;
    auto map = settings::incSettingsGet<settings::SettingsStringMap>(
        compat::RetainedString::fromStaticLiteral("Shortcuts"));
    if (map.empty()) return;
    auto all = commandIdMap();
    for (auto [key, shortcut] : map) {
        const auto found = all.findRetained(key.retainedString());
        if (found && !shortcut.empty()) {
            auto command = *found;
            ngClearShortcutFor(command);
            ngRegisterShortcut(shortcut.retainedString(),command,false);
        }
    }
}
}
