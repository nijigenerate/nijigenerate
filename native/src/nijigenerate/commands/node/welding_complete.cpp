#include "welding.hpp"
#include "welding_commands.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/panels/inspector/part.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <resource.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <string>
#include <stdexcept>
#include <utility>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
namespace nijigenerate::commands::node::welding {
namespace {
using Drawable=nicxlive::core::nodes::Drawable;
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
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/welding.d"),line);
}
std::shared_ptr<Drawable> source(const std::shared_ptr<Context>& context, std::size_t line) {
    enforce(compat::ngRequireObject(context)->hasNodes() && !context->nodes().empty(),
        "No source drawable in context", line);
    const auto nodes = context->nodes();
    if (nodes.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, nodes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/welding.d"), line + 1);
    auto result=std::dynamic_pointer_cast<Drawable>(nodes[0]);
    enforce(static_cast<bool>(result), "Context node is not a Drawable", line + 2);
    return result;
}
template<std::size_t N, std::size_t M>
detail::WeldingCommandText commandText(const char (&label)[N], const char (&description)[M]) {
    auto translatedLabel = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(label));
    auto translatedDescription = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(description));
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
template<class C, NodeWeldingCommand id, class... Args>
void registerIfConstructible(Args&&... args) {
    ngRegisterCommandIfSupported<id, C>(commands, std::forward<Args>(args)...);
}
template<class C, NodeWeldingCommand id, class... Args>
void registerExplicit(Args&&... args) {
    ngRegisterConstructedCommand<id, C>(commands, std::forward<Args>(args)...);
}
}
detail::WeldingCommandText AddWeldingCommand::sourceConstructorText() {
    return commandText("Add Welding", "Add welding link");
}
AddWeldingCommand::AddWeldingCommand(std::shared_ptr<Drawable> target,float weight)
    : AddWeldingCommand(sourceConstructorText(), target, weight) {}
AddWeldingCommand::AddWeldingCommand(detail::WeldingCommandText text,std::shared_ptr<Drawable> target,float weight)
    : ExCommand(std::move(text.label),std::move(text.description),target,weight) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AddWeldingCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.welding.AddWeldingCommand"));
}
void AddWeldingCommand::prepareSourceClassInfo() {
    ExCommand<AddWeldingArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AddWeldingCommand>();
}
detail::WeldingCommandText RemoveWeldingCommand::sourceConstructorText() {
    return commandText("Remove Welding", "Remove welding link");
}
RemoveWeldingCommand::RemoveWeldingCommand(std::shared_ptr<Drawable> target)
    : RemoveWeldingCommand(sourceConstructorText(), target) {}
RemoveWeldingCommand::RemoveWeldingCommand(detail::WeldingCommandText text,std::shared_ptr<Drawable> target)
    : ExCommand(std::move(text.label),std::move(text.description),target) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RemoveWeldingCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.welding.RemoveWeldingCommand"));
}
void RemoveWeldingCommand::prepareSourceClassInfo() {
    ExCommand<RemoveWeldingArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<RemoveWeldingCommand>();
}
detail::WeldingCommandText ChangeWeldingWeightCommand::sourceConstructorText() {
    return commandText("Change Welding Weight", "Change welding weight");
}
ChangeWeldingWeightCommand::ChangeWeldingWeightCommand(std::shared_ptr<Drawable> target,float weight)
    : ChangeWeldingWeightCommand(sourceConstructorText(), target, weight) {}
ChangeWeldingWeightCommand::ChangeWeldingWeightCommand(detail::WeldingCommandText text,
        std::shared_ptr<Drawable> target,float weight)
    : ExCommand(std::move(text.label),std::move(text.description),target,weight) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ChangeWeldingWeightCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.welding.ChangeWeldingWeightCommand"));
}
void ChangeWeldingWeightCommand::prepareSourceClassInfo() {
    ExCommand<ChangeWeldingWeightArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ChangeWeldingWeightCommand>();
}
std::shared_ptr<CommandResult> AddWeldingCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto drawable = source(context, 29);
    const auto targetValue = target;
    const auto weightValue = weight;
    panels::inspector::part::incRegisterWeldedPoints(drawable,targetValue,weightValue);
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> RemoveWeldingCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto drawable = source(context, 45);
    const auto targetValue = target;
    core::actionstack::incActionPush(std::allocate_shared<actions::node::DrawableRemoveWeldingAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::node::DrawableRemoveWeldingAction>{},
        drawable,targetValue,actions::node::Indices{},-1));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ChangeWeldingWeightCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto drawable=source(context, 62);
    std::ptrdiff_t index=-1;
    const auto links = drawable->weldedLinks;
    for(std::size_t i=0;i<links.size();++i)
        if(nicxlive::core::ngResourceObjectEquals(links[i].target.lock(), target)) {
            index=static_cast<std::ptrdiff_t>(i);
            break;
        }
    enforce(index>=0, "Welding link not found", 66);
    const auto currentLinks = drawable->weldedLinks;
    if (static_cast<std::size_t>(index) >= currentLinks.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, currentLinks.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/welding.d"), 67);
    auto indices=currentLinks[static_cast<std::size_t>(index)].indices;
    core::actionstack::incActionPush(actions::node::DrawableChangeWeldingAction::create(
        drawable, target, indices, weight));
    return CommandResult::opCall(true);
}
void ngInitWeldingCommands() {
    initialization::Program::run<initialization::Table>();
}
}
namespace nijigenerate::panels::inspector::part {
void ngRunPartWeldingCommand(commands::node::welding::NodeWeldingCommand id,std::shared_ptr<commands::Context> context,
    std::shared_ptr<Drawable> target,float weight) {
    namespace command=commands::node::welding;
    auto found=command::commands.find(id);
    if(found==command::commands.end()) throw std::runtime_error("No registered welding command");
    auto base=found->second;
    if(!commands::ngCommandAllowedInCurrentContext(base)) return;
    switch(id) {
    case command::NodeWeldingCommand::AddWelding: {
        auto value=std::dynamic_pointer_cast<command::AddWeldingCommand>(base);
        if(!value) throw std::runtime_error("Registered welding command type mismatch");
        value->target=target; value->weight=weight; break;
    }
    case command::NodeWeldingCommand::RemoveWelding: {
        auto value=std::dynamic_pointer_cast<command::RemoveWeldingCommand>(base);
        if(!value) throw std::runtime_error("Registered welding command type mismatch");
        value->target=target; break;
    }
    case command::NodeWeldingCommand::ChangeWeldingWeight: {
        auto value=std::dynamic_pointer_cast<command::ChangeWeldingWeightCommand>(base);
        if(!value) throw std::runtime_error("Registered welding command type mismatch");
        value->target=target; value->weight=weight; break;
    }
    }
    commands::ngRunCommand(base,context);
}
}
