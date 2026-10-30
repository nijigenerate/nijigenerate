#pragma once

#include <nijigenerate/panels/package.hpp>
#include <memory>

namespace nijigenerate::panels::animlist {
class AnimListPanel : public Panel, public std::enable_shared_from_this<AnimListPanel> {
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AnimListPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.animlist.AnimListPanel"));
            return true;
        }();
        (void)registered;
    }
    void onUpdate() override;

public:
    AnimListPanel();
    AnimListPanel(const AnimListPanel&) = delete;
    AnimListPanel& operator=(const AnimListPanel&) = delete;
};

void ngInitializeAnimListPanelModule();
} // namespace nijigenerate::panels::animlist

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::animlist::AnimListPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.animlist.AnimListPanel)";
    inline static constexpr char unqualifiedArgument[] = "(AnimListPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.animlist.AnimListPanel";
    inline static constexpr char unqualifiedElement[] = "AnimListPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::animlist::AnimListPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.animlist.AnimListPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.animlist.AnimListPanel)";
};
}
