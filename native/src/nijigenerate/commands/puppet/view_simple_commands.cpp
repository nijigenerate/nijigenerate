#include "view_simple_commands.hpp"
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/commands/additional_tables.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <runtime_state.hpp>
#include <compat/object_metadata.hpp>
#include <limits>

namespace nijigenerate::commands::puppet::view {
#define NG_VIEW_PREPARE(Type, Base) \
    void Type::prepareSourceClassInfo() { \
        Base::prepareSourceClassInfo(); \
        ::nijigenerate::commands::detail::prepareCommandResultClass<Type>(); \
    }
NG_VIEW_PREPARE(SetDefaultLayoutCommand, ExCommand<>)
NG_VIEW_PREPARE(ShowSaveScreenshotDialogCommand, ExCommand<>)
NG_VIEW_PREPARE(SaveScreenshotCommand, ExCommand<SaveScreenshotArguments>)
NG_VIEW_PREPARE(CaptureLiveScreenshotCommand, NativeBase)
NG_VIEW_PREPARE(ShowStatusForNerdsCommand, ExCommand<>)
NG_VIEW_PREPARE(ToggleDifferenceAggregationCommand, ExCommand<>)
#undef NG_VIEW_PREPARE
#define NG_VIEW_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view." #Type))
SetDefaultLayoutCommand::SetDefaultLayoutCommand()
    : SetDefaultLayoutCommand(SourceConstructorFrame::capture()) {}
SetDefaultLayoutCommand::SourceConstructorFrame SetDefaultLayoutCommand::SourceConstructorFrame::capture() {
    return {core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Reset Layout")),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Set default layout of panels."))};
}
SetDefaultLayoutCommand::SetDefaultLayoutCommand(SourceConstructorFrame frame)
    : ExCommand<>(std::move(frame.label), std::move(frame.description)) {
    NG_VIEW_CLASS_INFO(SetDefaultLayoutCommand);
}

std::shared_ptr<CommandResult> SetDefaultLayoutCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    core::ngSetDefaultLayout();
    return CommandResult::opCall(true);
}
ShowSaveScreenshotDialogCommand::ShowSaveScreenshotDialogCommand()
    : ShowSaveScreenshotDialogCommand(SourceConstructorFrame::capture()) {}
ShowSaveScreenshotDialogCommand::SourceConstructorFrame ShowSaveScreenshotDialogCommand::SourceConstructorFrame::capture() {
    return {core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Save Screenshot")),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Shows \"Save screenshot\" dialog."))};
}
ShowSaveScreenshotDialogCommand::ShowSaveScreenshotDialogCommand(SourceConstructorFrame frame)
    : ExCommand<>(std::move(frame.label), std::move(frame.description)) {
    NG_VIEW_CLASS_INFO(ShowSaveScreenshotDialogCommand);
}
std::shared_ptr<CommandResult> ShowSaveScreenshotDialogCommand::run(std::shared_ptr<Context> ctx) {
    const auto receiver = weak_from_this().lock();
    compat::Slice<io::TFD_Filter> filters;
    filters.resize(1);
    const compat::Slice<const char*> patterns{"*.png"};
    filters[0] = {{patterns.data(), patterns.size()}, "PNG Image (*.png)"};
    auto filename = io::incShowSaveDialog({filters.data(), filters.size()}, "",
        core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Save Screenshot...")));
    if (!filename.isNull()) {
        auto found = commands.find(ViewCommand::SaveScreenshot);
        if (found == commands.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/puppet/view.d"), 47);
        auto cmd = std::dynamic_pointer_cast<SaveScreenshotCommand>(found->second);
        if (cmd) { cmd->filename = filename; return ngRunCommand(cmd, ctx); }
    }
    return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("Screenshot save canceled"));
}
ShowStatusForNerdsCommand::ShowStatusForNerdsCommand()
    : ShowStatusForNerdsCommand(SourceConstructorFrame::capture()) {}
ShowStatusForNerdsCommand::SourceConstructorFrame ShowStatusForNerdsCommand::SourceConstructorFrame::capture() {
    return {core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Show Stats for Nerds")),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Show status for nerds."))};
}
ShowStatusForNerdsCommand::ShowStatusForNerdsCommand(SourceConstructorFrame frame)
    : ExCommand<>(std::move(frame.label), std::move(frame.description)) {
    NG_VIEW_CLASS_INFO(ShowStatusForNerdsCommand);
}
std::shared_ptr<CommandResult> ShowStatusForNerdsCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    core::ngShowStatsForNerds = !core::ngShowStatsForNerds;
    core::settings::incSettingsSet("NerdStats", core::ngShowStatsForNerds);
    return CommandResult::opCall(true);
}
ToggleDifferenceAggregationCommand::ToggleDifferenceAggregationCommand()
    : ToggleDifferenceAggregationCommand(SourceConstructorFrame::capture()) {}
ToggleDifferenceAggregationCommand::SourceConstructorFrame ToggleDifferenceAggregationCommand::SourceConstructorFrame::capture() {
    return {core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Difference Aggregation Debug")),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Toggle GPU difference aggregation for the selected node."))};
}
ToggleDifferenceAggregationCommand::ToggleDifferenceAggregationCommand(SourceConstructorFrame frame)
    : ExCommand<>(std::move(frame.label), std::move(frame.description)) {
    NG_VIEW_CLASS_INFO(ToggleDifferenceAggregationCommand);
}
#undef NG_VIEW_CLASS_INFO
std::shared_ptr<CommandResult> ToggleDifferenceAggregationCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    core::ngDifferenceAggregationDebugEnabled = !core::ngDifferenceAggregationDebugEnabled;
    if (!core::ngDifferenceAggregationDebugEnabled) {
        core::ngDifferenceAggregationResolvedIndex = std::numeric_limits<std::size_t>::max();
        core::ngDifferenceAggregationResultValid = false;
        core::ngDifferenceAggregationResult = core::ngDifferenceAggregationInitialResult();
        core::ngDifferenceAggregationResultSerial = 0;
        nicxlive::core::inSetDifferenceAggregationEnabled(false);
    }
    return CommandResult::opCall(true);
}
void ngInitViewCommands() {
    initialization::Program::run<initialization::Table>();
}
}
