#pragma once
#include "package.hpp"
namespace nijigenerate::panels::scene {
class ScenePanel : public Panel {
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ScenePanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.scene.ScenePanel"));
            return true;
        }();
        (void)registered;
    }
    void onUpdate() override;
public:
    ScenePanel();
};
void ngInitializeScenePanelModule();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::scene::ScenePanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.scene.ScenePanel)";
    inline static constexpr char unqualifiedArgument[] = "(ScenePanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.scene.ScenePanel";
    inline static constexpr char unqualifiedElement[] = "ScenePanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::scene::ScenePanel> {
    inline static constexpr char className[] = "nijigenerate.panels.scene.ScenePanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.scene.ScenePanel)";
};
}
