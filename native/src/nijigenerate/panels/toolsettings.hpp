#pragma once
#include "package.hpp"
namespace nijigenerate::panels::toolsettings {
class ToolSettingsPanel : public Panel {
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ToolSettingsPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.toolsettings.ToolSettingsPanel"));
            return true;
        }();
        (void)registered;
    }
    void onUpdate() override;
public:
    ToolSettingsPanel();
};
void ngInitializeToolSettingsPanelModule();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::toolsettings::ToolSettingsPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.toolsettings.ToolSettingsPanel)";
    inline static constexpr char unqualifiedArgument[] = "(ToolSettingsPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.toolsettings.ToolSettingsPanel";
    inline static constexpr char unqualifiedElement[] = "ToolSettingsPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::toolsettings::ToolSettingsPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.toolsettings.ToolSettingsPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.toolsettings.ToolSettingsPanel)";
};
}
