#include "mask_commands.hpp"
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <string>
#include <utility>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands::node::mask {
using Drawable = nicxlive::core::nodes::Drawable;
using Part = nicxlive::core::nodes::Part;
using MaskingMode = nicxlive::core::nodes::MaskingMode;
namespace detail {
template<std::size_t N, std::size_t M>
MaskCommandText commandText(const char (&label)[N], const char (&description)[M]) {
    auto translatedLabel = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(label));
    auto translatedDescription = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(description));
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
}
namespace {
template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (condition) return;
    compat::RetainedString copy;
    try {
        copy = compat::RetainedString(std::string_view(message,N-1));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(copy,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/mask.d"),line);
}
std::shared_ptr<Part> targetPart(const std::shared_ptr<Context>& context, std::size_t line) {
    enforce(compat::ngRequireObject(context)->hasNodes() && !context->nodes().empty(),
        "No target part in context", line);
    const auto nodes = context->nodes();
    if (nodes.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, nodes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/mask.d"), line + 1);
    const auto target = std::dynamic_pointer_cast<Part>(nodes[0]);
    enforce(static_cast<bool>(target), "Context node is not a Part", line + 2);
    return target;
}
}
#define NG_MASK_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.mask." #Type))
detail::MaskCommandText AddMaskCommand::sourceConstructorText() {
    return detail::commandText("Add Mask", "Add mask source to part");
}
AddMaskCommand::AddMaskCommand(std::shared_ptr<Drawable> source, MaskingMode masking)
    : AddMaskCommand(sourceConstructorText(), std::move(source), masking) {}
AddMaskCommand::AddMaskCommand(detail::MaskCommandText text, std::shared_ptr<Drawable> source, MaskingMode masking)
    : ExCommand(std::move(text.label), std::move(text.description), source, masking) { NG_MASK_CLASS_INFO(AddMaskCommand); }
detail::MaskCommandText RemoveMaskCommand::sourceConstructorText() {
    return detail::commandText("Remove Mask", "Remove mask source from part");
}
RemoveMaskCommand::RemoveMaskCommand(std::shared_ptr<Drawable> source)
    : RemoveMaskCommand(sourceConstructorText(), std::move(source)) {}
RemoveMaskCommand::RemoveMaskCommand(detail::MaskCommandText text, std::shared_ptr<Drawable> source)
    : ExCommand(std::move(text.label), std::move(text.description), source) { NG_MASK_CLASS_INFO(RemoveMaskCommand); }
detail::MaskCommandText ChangeMaskModeCommand::sourceConstructorText() {
    return detail::commandText("Change Mask Mode", "Change mask mode");
}
ChangeMaskModeCommand::ChangeMaskModeCommand(std::shared_ptr<Drawable> source, MaskingMode masking)
    : ChangeMaskModeCommand(sourceConstructorText(), std::move(source), masking) {}
ChangeMaskModeCommand::ChangeMaskModeCommand(detail::MaskCommandText text,
        std::shared_ptr<Drawable> source, MaskingMode masking)
    : ExCommand(std::move(text.label), std::move(text.description), source, masking) {
    NG_MASK_CLASS_INFO(ChangeMaskModeCommand);
}
#undef NG_MASK_CLASS_INFO
void AddMaskCommand::prepareSourceClassInfo() {
    ExCommand<AddMaskArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AddMaskCommand>();
}
void RemoveMaskCommand::prepareSourceClassInfo() {
    ExCommand<RemoveMaskArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<RemoveMaskCommand>();
}
void ChangeMaskModeCommand::prepareSourceClassInfo() {
    ExCommand<ChangeMaskModeArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ChangeMaskModeCommand>();
}
std::shared_ptr<CommandResult> AddMaskCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    const auto target = targetPart(context, 27);
    core::actionstack::incActionPush(std::allocate_shared<actions::node::PartAddMaskAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::node::PartAddMaskAction>{}, maskSrc, target, mode));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> RemoveMaskCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    const auto target = targetPart(context, 43);
    const auto index = target->getMaskIdx(maskSrc);
    enforce(index >= 0, "Mask entry not found", 47);
    const auto maskIndex = static_cast<std::size_t>(index);
    const auto maskCount = target->masks.size();
    if (maskIndex >= maskCount) throw compat::ngStaticError<compat::ArrayIndexError>(maskIndex, maskCount,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/mask.d"), 48);
    const auto masking = target->masks[maskIndex].mode;
    core::actionstack::incActionPush(std::allocate_shared<actions::node::PartRemoveMaskAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::node::PartRemoveMaskAction>{}, maskSrc, target, masking));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ChangeMaskModeCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    const auto target = targetPart(context, 63);
    core::actionstack::incActionPush(std::allocate_shared<actions::node::PartChangeMaskModeAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::node::PartChangeMaskModeAction>{}, target, maskSrc, mode));
    return CommandResult::opCall(true);
}
void ngInitNodeMaskCommands() {
    initialization::Program::run<initialization::Table>();
}
}
