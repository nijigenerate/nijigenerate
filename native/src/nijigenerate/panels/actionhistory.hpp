#pragma once

#include "package.hpp"
#include <memory>

namespace nijigenerate::panels::actionhistory {

class ActionHistoryPanel : public Panel, public std::enable_shared_from_this<ActionHistoryPanel> {
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ActionHistoryPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.actionhistory.ActionHistoryPanel"));
            return true;
        }();
        (void)registered;
    }
    void onUpdate() override;
public:
    ActionHistoryPanel();
    ActionHistoryPanel(const ActionHistoryPanel&) = delete;
    ActionHistoryPanel& operator=(const ActionHistoryPanel&) = delete;
};

void ngInitializeActionHistoryPanelModule();

} // namespace nijigenerate::panels::actionhistory

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::actionhistory::ActionHistoryPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.actionhistory.ActionHistoryPanel)";
    inline static constexpr char unqualifiedArgument[] = "(ActionHistoryPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.actionhistory.ActionHistoryPanel";
    inline static constexpr char unqualifiedElement[] = "ActionHistoryPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::actionhistory::ActionHistoryPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.actionhistory.ActionHistoryPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.actionhistory.ActionHistoryPanel)";
};
}
