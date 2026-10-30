#pragma once
#include "panel_command_map.hpp"
#include "toggle_panel_command.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <concepts>

namespace nijigenerate::commands::view::panel {
std::shared_ptr<Command> ensureTogglePanelCommand(std::shared_ptr<::nijigenerate::panels::Panel> panel);

namespace initialization {
consteval bool supported() {
    return ngCommandFactorySupported<TogglePanelVisibilityCommand>() &&
        requires(PanelCommandMap& map, PanelKey key, std::shared_ptr<TogglePanelVisibilityCommand> command,
                 std::shared_ptr<::nijigenerate::panels::Panel> panel) {
            command->panel = panel;
            map.set(key, command);
        };
}
}

template<class T> requires (std::same_as<T, PanelKey> && initialization::supported())
void ngInitCommands();
extern template void ngInitCommands<PanelKey>();
} // namespace nijigenerate::commands::view::panel

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<view::panel::PanelKey> {
    static consteval bool supported() { return view::panel::initialization::supported(); }
    template<class ActualKey = view::panel::PanelKey>
        requires (std::same_as<ActualKey, view::panel::PanelKey> && supported())
    static void invoke() { view::panel::ngInitCommands<ActualKey>(); }
};
}
