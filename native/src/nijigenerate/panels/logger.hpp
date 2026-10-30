#pragma once
#include "package.hpp"

namespace nijigenerate::panels::logger {
class LoggerPanel : public Panel {
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(LoggerPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.logger.LoggerPanel"));
            return true;
        }();
        (void)registered;
    }
    void onUpdate() override;
public:
    LoggerPanel();
};

void ngInitializeLoggerPanelModule();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::logger::LoggerPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.logger.LoggerPanel)";
    inline static constexpr char unqualifiedArgument[] = "(LoggerPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.logger.LoggerPanel";
    inline static constexpr char unqualifiedElement[] = "LoggerPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::logger::LoggerPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.logger.LoggerPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.logger.LoggerPanel)";
};
}
