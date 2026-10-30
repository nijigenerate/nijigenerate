#include "toolsettings.hpp"
#include <nijigenerate/viewport/main_viewport.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
namespace nijigenerate::panels::toolsettings {
ToolSettingsPanel::ToolSettingsPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("Tool Settings"),
        core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Tool Settings")), false) {}
void ToolSettingsPanel::onUpdate() { compat::ngRequireObject(viewport::base::ngViewport())->toolSettings(); }
void ngInitializeToolSettingsPanelModule() { ngInitializePanelModule<ToolSettingsPanel>(); }
}
