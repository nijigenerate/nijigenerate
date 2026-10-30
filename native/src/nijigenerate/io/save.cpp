#include "save.hpp"
#include "package.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/windows/welcome.hpp>
#include <nijigenerate/windows/base.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::io::save {
using namespace project;
using namespace windows::base;
using namespace widgets::dialog;
using core::translation::ngTranslateRetained;
bool incIsProjectModified() { return core::actionstack::incActionIsModified(); }
bool incFileOpen() {
    compat::Slice<TFD_Filter> filters;
    filters.resize(1);
    const compat::Slice<const char*> patterns{"*.inx"};
    filters[0] = {{patterns.data(), patterns.size()}, "nijigenerate Project (*.inx)"};
    auto file = incShowOpenDialog({filters.data(), filters.size()}, ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Open...")));
    return file.isNull() ? false : incOpenProject(file);
}
bool incFileSave() {
    incPopWelcomeWindow();
    if (!incProjectPath().empty()) return incSaveProject(incProjectPath());
    compat::Slice<TFD_Filter> filters;
    filters.resize(1);
    const compat::Slice<const char*> patterns{"*.inx"};
    filters[0] = {{patterns.data(), patterns.size()}, "nijigenerate Project (*.inx)"};
    auto file = incShowSaveDialog({filters.data(), filters.size()}, compat::RetainedString::emptyLiteral(),
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Save...")));
    return file.isNull() ? false : incSaveProject(file);
}
void incFileSaveAs() {
    incPopWelcomeWindow();
    compat::Slice<TFD_Filter> filters;
    filters.resize(1);
    const compat::Slice<const char*> patterns{"*.inx"};
    filters[0] = {{patterns.data(), patterns.size()}, "nijigenerate Project (*.inx)"};
    auto filename = !incProjectPath().empty() ? core::settings::SettingsString(incProjectPath())
                                            : core::settings::SettingsString::emptyLiteral();
    auto file = incShowSaveDialog({filters.data(), filters.size()}, filename, ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Save As...")));
    if (!file.isNull()) incSaveProject(file);
}
void incNewProjectAsk() {
    std::allocate_shared<NewProjectAskHandler>(
        nicxlive::core::d_runtime::SourceAllocator<NewProjectAskHandler>{})->start();
}
void incExitSaveAsk() {
    std::allocate_shared<ExitAskHandler>(
        nicxlive::core::d_runtime::SourceAllocator<ExitAskHandler>{})->start();
}
void incCloseProjectAsk() {
    std::allocate_shared<CloseAskHandler>(
        nicxlive::core::d_runtime::SourceAllocator<CloseAskHandler>{})->start();
}
void incCloseProject() {
    incNewProject();
    incPushWindow(ngCreateWindow<windows::welcome::WelcomeWindow>());
}
CloseAskHandler::CloseAskHandler() : DialogHandler(DialogTag::borrowed(DefaultDialogName)) {}
bool CloseAskHandler::onClickYes() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (incFileSave()) onProjectClose();
    return true;
}
bool CloseAskHandler::onClickNo() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    onProjectClose();
    return true;
}
void CloseAskHandler::onProjectClose() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    incCloseProject();
}
void NewProjectAskHandler::onProjectClose() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    incNewProject();
}
void ExitAskHandler::onProjectClose() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    core::ngExit();
}
void CloseAskHandler::show() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto tag = DialogTag::borrowed(INC_EXIT_ASK_DIALOG_NAME);
    const auto title = core::translation::ngTranslateCString("Save before closing?");
    const auto body = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "Would you like to save your changes before closing?\n\nYou can change the default behaviour in the settings."));
    ngDialog(tag, DialogTag::retained(title.data(), title.owner),
        DialogText(body),
        DialogLevel::Info, DialogButtons::Yes | DialogButtons::No | DialogButtons::Cancel);
}
void CloseAskHandler::start() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!incIsProjectModified()) { onClickNo(); return; }
    const auto setting = incGetSaveProjectOnClose();
    if (setting.view() == "dontSave") { onClickNo(); return; }
    if (setting.view() == "Save") { onClickYes(); return; }
    if (setting.view() == "Ask") { registerHandler(); show(); return; }
    throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Invalid save project on close setting"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/save.d"), 200);
}
}
