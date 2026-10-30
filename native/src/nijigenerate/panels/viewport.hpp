#pragma once
#include "package.hpp"
#include <limits>
#include <memory>
namespace nijigenerate::panels::viewport {
class ViewportPanel : public Panel, public std::enable_shared_from_this<ViewportPanel> {
    ImVec2 lastSize{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    bool actingInViewport = false;
    ImVec2 priorWindowPadding{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ViewportPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.viewport.ViewportPanel"));
            return true;
        }();
        (void)registered;
    }
    void onBeginUpdate() override;
    void onEndUpdate() override;
    void onUpdate() override;
public:
    ViewportPanel();
};
void ngInitializeViewportPanelModule();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::viewport::ViewportPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.viewport.ViewportPanel)";
    inline static constexpr char unqualifiedArgument[] = "(ViewportPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.viewport.ViewportPanel";
    inline static constexpr char unqualifiedElement[] = "ViewportPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::viewport::ViewportPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.viewport.ViewportPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.viewport.ViewportPanel)";
};
}
