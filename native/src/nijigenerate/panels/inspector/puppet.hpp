#pragma once
#include "common.hpp"

namespace nijigenerate::panels::inspector::puppet {
class PuppetInspector : public BaseInspector<ModelEditSubMode::Layout, Node> {
protected:
    void prepareSourceClassInfo() override {
        BaseInspector<ModelEditSubMode::Layout, Node>::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectTypeNames<PuppetInspector>(
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.inspector.puppet.PuppetInspector"),
                compat::RetainedString::fromStaticLiteral("(nijigenerate.panels.inspector.puppet.PuppetInspector)"));
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(PuppetInspector),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.inspector.puppet.PuppetInspector"));
            return true;
        }();
        (void)registered;
    }
public:
    // The standard ngCreateInspector factory executes the inherited source constructor.
    PuppetInspector() = default;
    void inspect(std::shared_ptr<Parameter> parameter = {}, Cursor cursor = {}) override;
    void run() override;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::inspector::puppet::PuppetInspector> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.panels.inspector.puppet.PuppetInspector)";
    inline static constexpr char unqualifiedArgument[] = "(PuppetInspector)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.panels.inspector.puppet.PuppetInspector";
    inline static constexpr char unqualifiedElement[] = "PuppetInspector";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::inspector::puppet::PuppetInspector> {
    inline static constexpr char className[] = "nijigenerate.panels.inspector.puppet.PuppetInspector";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.panels.inspector.puppet.PuppetInspector)";
};
}
