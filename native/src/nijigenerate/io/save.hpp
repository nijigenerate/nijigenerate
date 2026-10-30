#pragma once
#include "save_settings.hpp"
#include <nijigenerate/widgets/dialog.hpp>
#include <common/object_key_policy.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::io::save {
bool incIsProjectModified();
bool incFileOpen();
bool incFileSave();
void incFileSaveAs();
void incNewProjectAsk();
void incExitSaveAsk();
void incCloseProjectAsk();
void incCloseProject();
class CloseAskHandler : public widgets::dialog::DialogHandler {
public:
    inline static constexpr char DefaultDialogName[] = "CloseAskDialog";
    const char* INC_EXIT_ASK_DIALOG_NAME = DefaultDialogName;
    CloseAskHandler();
    bool onClickYes() override;
    bool onClickNo() override;
    virtual void onProjectClose();
    virtual void show();
    virtual void start();
protected:
    void prepareSourceClassInfo() override {
        widgets::dialog::DialogHandler::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(CloseAskHandler),
                compat::RetainedString::fromStaticLiteral("nijigenerate.io.save.CloseAskHandler"));
            return true;
        }();
        (void)registered;
    }
};
class NewProjectAskHandler : public CloseAskHandler {
public:
    void onProjectClose() override;
protected:
    void prepareSourceClassInfo() override {
        CloseAskHandler::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(NewProjectAskHandler),
                compat::RetainedString::fromStaticLiteral("nijigenerate.io.save.NewProjectAskHandler"));
            return true;
        }();
        (void)registered;
    }
};
class ExitAskHandler : public CloseAskHandler {
public:
    void onProjectClose() override;
protected:
    void prepareSourceClassInfo() override {
        CloseAskHandler::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExitAskHandler),
                compat::RetainedString::fromStaticLiteral("nijigenerate.io.save.ExitAskHandler"));
            return true;
        }();
        (void)registered;
    }
};
}

namespace nicxlive::core::d_runtime {
#define NG_IO_SAVE_CLASS_SOURCE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::io::save::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.save." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.io.save." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }; \
    template<> struct SourceObjectTypeNames<::nijigenerate::io::save::Type> { \
        inline static constexpr char className[] = "nijigenerate.io.save." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.io.save." #Type ")"; \
    }
NG_IO_SAVE_CLASS_SOURCE_ORIGIN(CloseAskHandler);
NG_IO_SAVE_CLASS_SOURCE_ORIGIN(NewProjectAskHandler);
NG_IO_SAVE_CLASS_SOURCE_ORIGIN(ExitAskHandler);
#undef NG_IO_SAVE_CLASS_SOURCE_ORIGIN
}
