#include "editor_commands.hpp"
#include "editor_table.hpp"
#include <nijigenerate/viewport/depth/mesheditor/editor.hpp>
#include <nijigenerate/viewport/depth/mesheditor/action.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <bit>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <compat/object_metadata.hpp>
#include <utility>

namespace nijigenerate::commands::depth::editor {
using Editor = ::nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor;
using Target = ::nijigenerate::viewport::depth::mesheditor::node::DepthMeshEditorOne;
using Operation = ::nijigenerate::viewport::depth::tools::operation::DepthOperation;
using Operations = compat::Slice<std::shared_ptr<Operation>>;
namespace detail {
template<std::size_t N, std::size_t M>
DepthCommandText commandText(const char (&label)[N], const char (&description)[M]) {
    auto translatedLabel = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(label));
    auto translatedDescription = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(description));
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
}
namespace {
std::shared_ptr<CommandResult> replaceEditorDepthOpsWithUndo(std::shared_ptr<Editor> editor,
    std::shared_ptr<Target> target, Operations next, std::ptrdiff_t selected) {
    if (!editor || !target) return CommandResult::opCall(false);
    auto action = ::nijigenerate::viewport::depth::mesheditor::action::DepthOperationListChangeAction::create(editor, target);
    editor->replaceOperations(target, next);
    editor->selectOperation(target, selected);
    action->updateNewState();
    core::actionstack::incActionPush(action);
    return CommandResult::opCall(true);
}
Operations sourceSlice(Operations values, std::size_t first, std::size_t last, std::size_t line) {
    if (first > last || last > values.size()) throw compat::ngStaticError<compat::ArraySliceError>(
        first, last, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/editor.d"), line);
    return values.slice(first, last);
}
Operations without(Operations values, std::size_t index, std::size_t line) {
    const auto prefix = sourceSlice(values, 0, index, line);
    const auto suffix = sourceSlice(values, index + 1, values.size(), line);
    Operations result;
    result.resize(prefix.size() + suffix.size());
    std::size_t output = 0;
    for (auto value : prefix) result[output++] = std::move(value);
    for (auto value : suffix) result[output++] = std::move(value);
    return result;
}
Operations inserted(Operations values, std::size_t index, std::shared_ptr<Operation> operation) {
    const auto prefix = sourceSlice(values, 0, index, 61);
    const auto suffix = sourceSlice(values, index, values.size(), 61);
    Operations result;
    result.resize(values.size() + 1);
    std::size_t output = 0;
    for (auto value : prefix) result[output++] = std::move(value);
    result[output++] = std::move(operation);
    for (auto value : suffix) result[output++] = std::move(value);
    return result;
}
}
#define NG_DEPTH_EDITOR_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.editor." #Type))
detail::DepthCommandText AddEditorDepthOpCommand::sourceConstructorText() {
    return detail::commandText("Add Editor Depth Operation", "Add one in-progress depth operation");
}
AddEditorDepthOpCommand::AddEditorDepthOpCommand()
    : AddEditorDepthOpCommand(sourceConstructorText()) {}
AddEditorDepthOpCommand::AddEditorDepthOpCommand(detail::DepthCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) { NG_DEPTH_EDITOR_CLASS_INFO(AddEditorDepthOpCommand); }
detail::DepthCommandText UpdateEditorDepthOpCommand::sourceConstructorText() {
    return detail::commandText("Update Editor Depth Operation", "Replace one in-progress depth operation");
}
UpdateEditorDepthOpCommand::UpdateEditorDepthOpCommand()
    : UpdateEditorDepthOpCommand(sourceConstructorText()) {}
UpdateEditorDepthOpCommand::UpdateEditorDepthOpCommand(detail::DepthCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) {
    NG_DEPTH_EDITOR_CLASS_INFO(UpdateEditorDepthOpCommand);
}
detail::DepthCommandText RemoveEditorDepthOpCommand::sourceConstructorText() {
    return detail::commandText("Remove Editor Depth Operation", "Remove one in-progress depth operation");
}
RemoveEditorDepthOpCommand::RemoveEditorDepthOpCommand()
    : RemoveEditorDepthOpCommand(sourceConstructorText()) {}
RemoveEditorDepthOpCommand::RemoveEditorDepthOpCommand(detail::DepthCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) {
    NG_DEPTH_EDITOR_CLASS_INFO(RemoveEditorDepthOpCommand);
}
#undef NG_DEPTH_EDITOR_CLASS_INFO
void AddEditorDepthOpCommand::prepareSourceClassInfo() {
    ExCommand<AddEditorDepthOpArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AddEditorDepthOpCommand>();
}
void UpdateEditorDepthOpCommand::prepareSourceClassInfo() {
    ExCommand<UpdateEditorDepthOpArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<UpdateEditorDepthOpCommand>();
}
void RemoveEditorDepthOpCommand::prepareSourceClassInfo() {
    ExCommand<RemoveEditorDepthOpArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<RemoveEditorDepthOpCommand>();
}
std::shared_ptr<CommandResult> AddEditorDepthOpCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    if (!editorSet || !target || !operation) return CommandResult::opCall(false);
    auto list = compat::ngRequireObject(editorSet)->copyOperations(target);
    const auto existing = compat::ngRequireObject(editorSet)->indexOfOperationInstance(target, operation);
    if (existing >= 0) {
        const auto i = static_cast<std::size_t>(existing);
        list = without(list, i, 51);
        if (index < 0) index = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i));
    }
    std::ptrdiff_t selected;
    if (index < 0 || static_cast<std::size_t>(index) >= list.size()) {
        list.append(operation);
        selected = static_cast<std::ptrdiff_t>(list.size()) - 1;
    } else {
        const auto i = static_cast<std::size_t>(index);
        list = inserted(list, i, operation);
        selected = static_cast<std::ptrdiff_t>(i);
    }
    return replaceEditorDepthOpsWithUndo(editorSet, target, list, selected);
}
std::shared_ptr<CommandResult> UpdateEditorDepthOpCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    if (!editorSet || !target || !operation || index < 0) return CommandResult::opCall(false);
    auto list = compat::ngRequireObject(editorSet)->copyOperations(target);
    if (static_cast<std::size_t>(index) >= list.size()) return CommandResult::opCall(false);
    const auto slot = static_cast<std::size_t>(index);
    list[slot] = operation;
    return replaceEditorDepthOpsWithUndo(editorSet, target, list, index);
}
std::shared_ptr<CommandResult> RemoveEditorDepthOpCommand::run(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    if (!editorSet || !target || index < 0) return CommandResult::opCall(false);
    auto list = compat::ngRequireObject(editorSet)->copyOperations(target);
    if (static_cast<std::size_t>(index) >= list.size()) return CommandResult::opCall(false);
    list = without(list, static_cast<std::size_t>(index), 100);
    const auto selected = list.empty() ? std::ptrdiff_t(-1) : static_cast<std::ptrdiff_t>(list.size()) - 1;
    return replaceEditorDepthOpsWithUndo(editorSet, target, list, selected);
}
void ngInitDepthEditorCommands() {
    initialization::Program::run<initialization::Table>();
}
}
