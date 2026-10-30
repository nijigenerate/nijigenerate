#include "toggle_panel_command.hpp"
#include "nijigenerate/panels/package.hpp"
#include "nijigenerate/core/settings.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <string>

namespace nijigenerate::commands::view::panel {
compat::RetainedString TogglePanelVisibilityCommand::label() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return panel ? compat::ngRequireObject(panel)->displayName() : _label;
}

bool TogglePanelVisibilityCommand::runnable(std::shared_ptr<Context>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return panel && !panel->alwaysVisible && compat::ngRequireObject(panel)->isActive();
}

std::shared_ptr<CommandResult> TogglePanelVisibilityCommand::run(std::shared_ptr<Context>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!panel || panel->alwaysVisible)
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("Panel not toggleable"));
    panel->visible = !panel->visible;
    const auto name = compat::ngRequireObject(panel)->name();
    const auto key = core::settings::SettingsString(std::string(name.view()) + ".visible");
    const auto visible = compat::ngRequireObject(panel)->visible;
    core::settings::incSettingsSet(key, visible);
    return CommandResult::opCall(true);
}
}
