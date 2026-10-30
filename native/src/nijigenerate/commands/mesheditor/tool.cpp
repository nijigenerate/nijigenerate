// Independent commands/mesheditor/tool.d implementation. BSD-2-Clause.
#include "tool_commands.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/package.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <array>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands::mesheditor::tool {
namespace meshTools = ::nijigenerate::viewport::common::mesheditor::tools;
using Mode = meshTools::enums::VertexToolMode;
thread_local SelectToolModeCommandMap selectToolModeCommands;
namespace {
template<class Operation> auto allocateToolLabel(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
struct ModeSymbol { Mode value; const char* name; };
constexpr std::array modeSymbols{
    ModeSymbol{Mode::Points, "Points"}, ModeSymbol{Mode::Connect, "Connect"},
    ModeSymbol{Mode::PathDeform, "PathDeform"}, ModeSymbol{Mode::Grid, "Grid"},
    ModeSymbol{Mode::Brush, "Brush"}, ModeSymbol{Mode::BezierDeform, "BezierDeform"},
    ModeSymbol{Mode::AltGridDeform, "AltGridDeform"}, ModeSymbol{Mode::AltBezierDeform, "AltBezierDeform"},
    ModeSymbol{Mode::LassoSelection, "LassoSelection"}, ModeSymbol{Mode::EdgeCutter, "EdgeCutter"}};
std::string modeName(Mode mode) {
    for (const auto& symbol : modeSymbols) if (symbol.value == mode) return symbol.name;
    return "cast(VertexToolMode)" + std::to_string(static_cast<std::int32_t>(mode));
}
auto activeEditor() {
    auto editor = viewport::model::deform::ngViewportModelDeformGetEditor();
    if (!editor) editor = viewport::vertex::ngVertexViewportGetEditor();
    return editor;
}
}
SelectToolModeCommand::SourcePresentation SelectToolModeCommand::sourceConstructorPresentation() {
    return {core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Select mesh editor tool mode"))};
}
SelectToolModeCommand::SelectToolModeCommand(Mode value)
    : SelectToolModeCommand(sourceConstructorPresentation(), value) {}
SelectToolModeCommand::SelectToolModeCommand(SourcePresentation presentation, Mode value)
    : ExCommand(nullptr, presentation.description, value) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SelectToolModeCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.mesheditor.tool.SelectToolModeCommand"));
    SourceAfterBaseProgram::initialize(*this);
}
void SelectToolModeCommand::prepareSourceClassInfo() {
    ExCommand<SelectToolModeArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SelectToolModeCommand>();
}
std::shared_ptr<CommandResult> SelectToolModeCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    auto editor = activeEditor();
    if (!editor) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No editor available"));
    compat::ngRequireObject(editor)->setToolMode(mode);
    return CommandResult::opCall(true);
}
bool SelectToolModeCommand::runnable(std::shared_ptr<Context>) {
    const auto owner = weak_from_this().lock();
    auto editor = activeEditor();
    if (!editor || !info) return false;
    const auto receiver = info;
    const auto deformOnly = editor->deformOnly;
    const auto targets = editor->getTargets();
    return compat::ngRequireObject(receiver)->canUse(deformOnly, targets);
}
void SelectToolModeCommand::_init() {
    SourceInitializationProgram::initialize(*this);
}
void SelectToolModeCommand::SourceInitializationProgram::initialize(SelectToolModeCommand& target) {
    const auto receiver = target.weak_from_this().lock();
    const auto infos = meshTools::incGetToolInfo();
    for (auto current : infos) {
        const auto currentMode = compat::ngRequireObject(current)->mode();
        if (currentMode == target.mode) {
            const auto description = compat::ngRequireObject(current)->description();
            auto label = allocateToolLabel([&] {
                return compat::RetainedString("Select Tool: " + std::string(description.view()));
            });
            SourceCommandAssignment<compat::RetainedString, compat::RetainedString&>::assign(target._label, label);
        }
    }
    // The source deliberately overwrites the preceding localized description.
    auto label = allocateToolLabel([&] { return compat::RetainedString("Select Tool: " + modeName(target.mode)); });
    SourceCommandAssignment<compat::RetainedString, compat::RetainedString&>::assign(target._label, label);
    auto info = meshTools::ngGetToolInfoOf(target.mode);
    SourceCommandAssignment<InfoRef, InfoRef&>::assign(target.info, info);
}
std::shared_ptr<Command> ngEnsureSelectToolModeCommand(Mode mode) {
    if (const auto found = selectToolModeCommands.find(mode); found != selectToolModeCommands.end())
        return found->second;
    auto command = ngCreateRegisteredCommand<SelectToolModeCommand>(mode);
    selectToolModeCommands[mode] = command;
    return command;
}
void ngInitSelectToolModeCommands() {
    for (const auto& symbol : modeSymbols) ngEnsureSelectToolModeCommand(symbol.value);
    const auto infos = meshTools::incGetToolInfo();
    for (auto info : infos) ngEnsureSelectToolModeCommand(compat::ngRequireObject(info)->mode());
}
}
