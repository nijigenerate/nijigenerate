#include "edit_commands.hpp"
#include "edit_table.hpp"
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/windows/settings.hpp>
#include <nijigenerate/windows/command_browser.hpp>
#include <compat/object_metadata.hpp>
#include <utility>

namespace nijigenerate::commands::puppet::edit {
namespace detail {
template<std::size_t N, std::size_t M>
EditCommandText commandText(const char (&label)[N], const char (&description)[M]) {
    auto translatedLabel = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(label));
    auto translatedDescription = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(description));
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
}
#define NG_EDIT_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.edit." #Type))
detail::EditCommandText UndoCommand::sourceConstructorText() {
    return detail::commandText("Undo", "Undo last action");
}
UndoCommand::UndoCommand() : UndoCommand(sourceConstructorText()) {}
UndoCommand::UndoCommand(detail::EditCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) { NG_EDIT_CLASS_INFO(UndoCommand); }
detail::EditCommandText RedoCommand::sourceConstructorText() {
    return detail::commandText("Redo", "Redo previously undone action");
}
RedoCommand::RedoCommand() : RedoCommand(sourceConstructorText()) {}
RedoCommand::RedoCommand(detail::EditCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) { NG_EDIT_CLASS_INFO(RedoCommand); }
ShowSettingsWindowCommand::ShowSettingsWindowCommand()
    : ShowSettingsWindowCommand(sourceConstructorText()) {}
detail::EditCommandText ShowSettingsWindowCommand::sourceConstructorText() {
    return detail::commandText("Settings", "Show settings window");
}
ShowSettingsWindowCommand::ShowSettingsWindowCommand(detail::EditCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) { NG_EDIT_CLASS_INFO(ShowSettingsWindowCommand); }
ShowCommandBrowserWindowCommand::ShowCommandBrowserWindowCommand()
    : ShowCommandBrowserWindowCommand(sourceConstructorText()) {}
detail::EditCommandText ShowCommandBrowserWindowCommand::sourceConstructorText() {
    return detail::commandText("Command Browser", "Inspect available commands and their inputs/outputs.");
}
ShowCommandBrowserWindowCommand::ShowCommandBrowserWindowCommand(detail::EditCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) { NG_EDIT_CLASS_INFO(ShowCommandBrowserWindowCommand); }
#undef NG_EDIT_CLASS_INFO
void UndoCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<UndoCommand>();
}
void RedoCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<RedoCommand>();
}
void ShowSettingsWindowCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ShowSettingsWindowCommand>();
}
void ShowCommandBrowserWindowCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ShowCommandBrowserWindowCommand>();
}
std::shared_ptr<CommandResult> UndoCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    core::actionstack::incActionUndo();
    return CommandResult::opCall(true);
}
bool UndoCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return core::actionstack::incActionCanUndo();
}
std::shared_ptr<CommandResult> RedoCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    core::actionstack::incActionRedo();
    return CommandResult::opCall(true);
}
bool RedoCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return core::actionstack::incActionCanRedo();
}
std::shared_ptr<CommandResult> ShowSettingsWindowCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    if (!windows::settings::ngIsSettingsOpen())
        windows::base::incPushWindow(windows::base::ngCreateWindow<windows::settings::SettingsWindow>());
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ShowCommandBrowserWindowCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    if (!windows::command_browser::incIsCommandBrowserOpen)
        windows::base::incPushWindow(windows::base::ngCreateWindow<windows::command_browser::CommandBrowserWindow>());
    return CommandResult::opCall(true);
}
void ngInitEditCommands() {
    initialization::Program::run<initialization::Table>();
}
}
