#include "panel_registry.hpp"
#include "nijigenerate/panels/package.hpp"

namespace nijigenerate::commands::view::panel {
std::shared_ptr<Command> ensureTogglePanelCommand(std::shared_ptr<::nijigenerate::panels::Panel> panel) {
    if (!panel) return {};
    PanelKey key{panel->name()};
    if (const auto found = togglePanelCommands.find(key)) return *found;
    auto command = ngCreateRegisteredCommand<TogglePanelVisibilityCommand>();
    command->panel = panel;
    togglePanelCommands.set(std::move(key), command);
    return command;
}

template<class T> requires (std::same_as<T, PanelKey> && initialization::supported())
void ngInitCommands() {
    // Copy the range descriptor once, as the source foreach does.
    const auto panels = ::nijigenerate::panels::incPanels;
    for (const auto panel : panels) ensureTogglePanelCommand(panel);
}
template void ngInitCommands<PanelKey>();
} // namespace nijigenerate::commands::view::panel
