#include "command_browser_enum_traits.hpp"
#include "command_browser_value_traits.hpp"
#include "command_browser_result_traits.hpp"
#include <nijigenerate/commands/all_command_maps.hpp>
#include <type_traits>
#include <utility>

namespace nijigenerate::windows::command_browser::detail {
compat::Slice<CommandInfo> gCommandInfos;
BrowserClassMap<CommandRef, CommandInfo> gCommandInfosByCmd;

void rebuildCommandInfos() {
    gCommandInfos.resize(0);
    gCommandInfosByCmd.clear();
    commands::ngForEachCommandMap([](const auto& table) {
        using Map = std::remove_cvref_t<decltype(table)>;
        using Entry = std::remove_cvref_t<decltype(*std::declval<const Map&>().begin())>;
        using Key = std::remove_cv_t<typename Entry::first_type>;
        using Value = typename Entry::second_type;
        // Retain the actual AA descriptor, not a key-type-selected other map.
        const auto current = table;
        for (auto [key, command] : current) {
            bool reflected = false;
            auto visitor = [&]<class C>(auto, CommandRef, std::string id) {
                reflected = true;
                CommandInfo info;
                info.id = text(id);
                info.category = staticText(commands::detail::CommandMapKeyMetadata<Key>::typeName);
                info.cmd = command;
                if constexpr (!std::same_as<C, void>)
                    reflectBrowserCommand<C, std::is_enum_v<Key>>(info);
                gCommandInfos.append(info);
                gCommandInfosByCmd[command] = info;
            };
            if constexpr (std::is_enum_v<Key>) {
                commands::ngVisitConcreteCommand(key, command, visitor);
                // The source still appends an empty metadata record when no
                // declared enum member matches the stored value.
                if (!reflected)
                    visitor.template operator()<void>(key, command, commands::ngCommandIdFromKey(key));
            } else {
                visitor.template operator()<typename Value::element_type>(
                    key, command, commands::ngCommandIdFromKey(key));
            }
        }
    });
}
}
