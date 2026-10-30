#pragma once
#include "parameters.hpp"
#include <nijigenerate/panels/package.hpp>
namespace nijigenerate::panels::parameters {
class ParametersPanel : public Panel, public std::enable_shared_from_this<ParametersPanel> {
    compat::RetainedString filter;
    compat::RetainedString grabParam=compat::RetainedString::emptyLiteral();
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParametersPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.parameters.ParametersPanel"));
            return true;
        }();
        (void)registered;
    }
    void onUpdate() override;
public:
    ParametersPanel();
    ParametersPanel(const ParametersPanel&) = delete;
    ParametersPanel& operator=(const ParametersPanel&) = delete;
};
void ngInitializeParametersPanel();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::parameters::ParametersPanel> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.panels.parameters.ParametersPanel)"),
            RetainedString::fromStaticLiteral("(ParametersPanel)"),
            RetainedString::fromStaticLiteral("nijigenerate.panels.parameters.ParametersPanel"),
            RetainedString::fromStaticLiteral("ParametersPanel")};
    }
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::parameters::ParametersPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.parameters.ParametersPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.parameters.ParametersPanel)";
};
}
