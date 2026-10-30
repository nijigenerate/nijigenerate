#pragma once

#include <nijigenerate/panels/package.hpp>

namespace nijigenerate::panels::armedparam {
class ArmedParametersPanel : public Panel, public std::enable_shared_from_this<ArmedParametersPanel> {
private:
    compat::RetainedString filter;
    compat::RetainedString grabParam = compat::RetainedString::emptyLiteral();

protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ArmedParametersPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.armedparam.ArmedParametersPanel"));
            return true;
        }();
        (void)registered;
    }
    void onUpdate() override;

public:
    ArmedParametersPanel();
    ArmedParametersPanel(const ArmedParametersPanel&) = delete;
    ArmedParametersPanel& operator=(const ArmedParametersPanel&) = delete;
    bool isActive() override;
};
void ngInitializeArmedParametersPanel();
} // namespace nijigenerate::panels::armedparam

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::armedparam::ArmedParametersPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.armedparam.ArmedParametersPanel)";
    inline static constexpr char unqualifiedArgument[] = "(ArmedParametersPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.armedparam.ArmedParametersPanel";
    inline static constexpr char unqualifiedElement[] = "ArmedParametersPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::armedparam::ArmedParametersPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.armedparam.ArmedParametersPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.armedparam.ArmedParametersPanel)";
};
}
