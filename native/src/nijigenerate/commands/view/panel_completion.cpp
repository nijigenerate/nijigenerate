#include "toggle_panel_command.hpp"
#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::commands::view::panel {
TogglePanelVisibilityCommand::TogglePanelVisibilityCommand()
    : TogglePanelVisibilityCommand(SourceConstructorFrame::capture()) {}

TogglePanelVisibilityCommand::SourceConstructorFrame
TogglePanelVisibilityCommand::SourceConstructorFrame::capture() {
    return {core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "Toggle visibility of the specified panel"))};
}

TogglePanelVisibilityCommand::TogglePanelVisibilityCommand(SourceConstructorFrame frame)
    : ExCommand(compat::RetainedString{},
        frame.description,
        std::shared_ptr<::nijigenerate::panels::Panel>{}) {}

compat::RetainedString TogglePanelVisibilityCommand::description() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(panel) {
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Toggle visibility of panel '%s'"));
        const auto name=compat::ngRequireObject(panel)->displayName();
        const auto argument=core::translation::ngFormatArgument(name);
        return compat::RetainedString(core::translation::ngFormatTyped(
            pattern.view(),std::span(&argument,1)));
    }
    return _desc;
}
}
