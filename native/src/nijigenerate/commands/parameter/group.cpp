// Independent source/nijigenerate/commands/parameter/group.d complete bodies.
#include "group_commands.hpp"
#include "group_table.hpp"
#include "base.hpp"
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::commands::parameter::group {
namespace {
// Materialize the Source array argument inside the actual Result constructor,
// after SourceAllocator has allocated that same Result object.
struct ResultGroupSliceArgument {
    const std::shared_ptr<ext::ExParameterGroup>& value;
    operator compat::Slice<std::shared_ptr<ext::ExParameterGroup>>() const { return {value}; }
};

template<std::size_t N> compat::RetainedString translated(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
base::ParameterRef parameterAtZero(const std::shared_ptr<Context>& context, std::size_t line) {
    const auto parameters = compat::ngRequireObject(context)->parameters();
    if (parameters.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, parameters.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/parameter/group.d"), line);
    return parameters[0];
}
template<class C> void registerValue(GroupCommand id, std::shared_ptr<C> command) {
    ngRegisterCommandMeta(command); commands[id] = std::move(command);
}
}
#define NG_GROUP_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.group." #Type))
MoveParameterCommand::MoveParameterCommand(std::shared_ptr<ext::ExParameterGroup> target, std::int32_t position)
    : MoveParameterCommand(SourceConstructorFrame::capture(target, position)) {}
MoveParameterCommand::SourceConstructorFrame MoveParameterCommand::SourceConstructorFrame::capture(
        std::shared_ptr<ext::ExParameterGroup>& target, std::int32_t& position) {
    return {translated("Move Parameter"), target, position};
}
MoveParameterCommand::MoveParameterCommand(SourceConstructorFrame frame)
    : ExCommand({}, std::move(frame.description), frame.group, frame.index) {
    NG_GROUP_CLASS_INFO(MoveParameterCommand);
}
CreateParamGroupCommand::CreateParamGroupCommand(std::int32_t position)
    : CreateParamGroupCommand(SourceConstructorFrame::capture(position)) {}
CreateParamGroupCommand::SourceConstructorFrame CreateParamGroupCommand::SourceConstructorFrame::capture(
        std::int32_t& position) {
    return {translated("Create Parameter Group"), position};
}
CreateParamGroupCommand::CreateParamGroupCommand(SourceConstructorFrame frame)
    : NativeBase({}, std::move(frame.description), frame.index) { NG_GROUP_CLASS_INFO(CreateParamGroupCommand); }
ChangeGroupColorCommand::ChangeGroupColorCommand(std::array<float, 3> value)
    : ChangeGroupColorCommand(SourceConstructorFrame::capture(value)) {}
ChangeGroupColorCommand::SourceConstructorFrame ChangeGroupColorCommand::SourceConstructorFrame::capture(
        std::array<float, 3>& value) {
    return {translated("Change Parameter Group Color"), value};
}
ChangeGroupColorCommand::ChangeGroupColorCommand(SourceConstructorFrame frame)
    : ExCommand({}, std::move(frame.description), frame.color) { NG_GROUP_CLASS_INFO(ChangeGroupColorCommand); }
DeleteParamGroupCommand::DeleteParamGroupCommand()
    : DeleteParamGroupCommand(SourceConstructorFrame::capture()) {}
DeleteParamGroupCommand::SourceConstructorFrame DeleteParamGroupCommand::SourceConstructorFrame::capture() {
    return {translated("Delete Parameter Group")};
}
DeleteParamGroupCommand::DeleteParamGroupCommand(SourceConstructorFrame frame)
    : NativeBase({}, std::move(frame.description)) { NG_GROUP_CLASS_INFO(DeleteParamGroupCommand); }
#undef NG_GROUP_CLASS_INFO
void MoveParameterCommand::prepareSourceClassInfo() {
    ExCommand<MoveParameterArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<MoveParameterCommand>();
}
void CreateParamGroupCommand::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<CreateParamGroupCommand>();
}
void ChangeGroupColorCommand::prepareSourceClassInfo() {
    ExCommand<ChangeGroupColorArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ChangeGroupColorCommand>();
}
void DeleteParamGroupCommand::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<DeleteParamGroupCommand>();
}
std::shared_ptr<CommandResult> MoveParameterCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, "No parameters");
    const auto parameter = std::dynamic_pointer_cast<ext::ExParameter>(parameterAtZero(context, 29));
    auto oldParent = parameter ? parameter->getParent() : nullptr;
    const auto movingParameter = parameterAtZero(context, 31);
    const auto destination = group;
    const auto position = index;
    base::incMoveParameter(movingParameter, destination, position);
    auto newParent = parameter ? parameter->getParent() : nullptr;
    if (parameter && oldParent != newParent)
        core::actionstack::incActionPush(actions::parameter::ParameterMoveAction::createSourceDeferred(
            [&] { return parameterAtZero(context, 34); },
            [&] { return oldParent; }, [&] { return newParent; }));
    return CommandResult::opCall(true);
}
std::shared_ptr<CreateParamGroupCommand::Result> CreateParamGroupCommand::run(
        const std::shared_ptr<Context>& context) { return runTyped(context); }
std::shared_ptr<CreateParamGroupCommand::Result> CreateParamGroupCommand::runTyped(std::shared_ptr<Context> context) {
    const auto commandOwner = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasPuppet()) return Result::opCall(false, {}, "No puppet");
    auto group = ext::ExParameterGroup::createSourceDeferred([] {
        return translated("New Parameter Group").toNullableString();
    });
    const auto puppet = std::dynamic_pointer_cast<ext::ExPuppet>(context->puppet());
    compat::ngRequireObject(puppet)->addGroup(group);
    core::actionstack::incActionPush(std::allocate_shared<actions::parameter::ParameterGroupAddAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::parameter::ParameterGroupAddAction>{}, puppet, group));
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultGroupSliceArgument{group}, "Parameter group created");
}
std::shared_ptr<CommandResult> ChangeGroupColorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty() ||
        !std::dynamic_pointer_cast<ext::ExParameterGroup>(parameterAtZero(context, 65)))
        return CommandResult::opCall(false, "No parameter group");
    auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(parameterAtZero(context, 67));
    const auto oldColor = compat::ngRequireObject(group)->color;
    group->color = {color[0], color[1], color[2]};
    using Color = std::remove_cvref_t<decltype(group->color)>;
    using Action = actions::parameter::ParameterValueChangeAction<Color>;
    core::actionstack::incActionPush(actions::parameter::ngCreateParameterValueAction<Action>(
        compat::RetainedString::fromStaticLiteral("color"), group, oldColor, group->color,
        std::shared_ptr<Color>(group, &group->color)));
    return CommandResult::opCall(true);
}
std::shared_ptr<DeleteParamGroupCommand::Result> DeleteParamGroupCommand::run(
        const std::shared_ptr<Context>& context) { return runTyped(context); }
std::shared_ptr<DeleteParamGroupCommand::Result> DeleteParamGroupCommand::runTyped(std::shared_ptr<Context> context) {
    const auto commandOwner = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty() ||
        !std::dynamic_pointer_cast<ext::ExParameterGroup>(parameterAtZero(context, 80)))
        return Result::opCall(false, {}, "No parameter group");
    const auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(parameterAtZero(context, 82));
    const auto puppet = std::dynamic_pointer_cast<ext::ExPuppet>(project::incActivePuppet());
    const auto action = std::allocate_shared<actions::parameter::ParameterGroupRemoveAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::parameter::ParameterGroupRemoveAction>{}, puppet, group);
    // Source foreach retains a shallow descriptor while setParent mutates its backing.
    const auto children = compat::ngRequireObject(group)->children;
    for (const auto child : children) {
        const auto extended = std::dynamic_pointer_cast<ext::ExParameter>(child);
        compat::ngRequireObject(extended)->setParent(nullptr);
    }
    compat::ngRequireObject(puppet)->removeGroup(group);
    core::actionstack::incActionPush(action);
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultGroupSliceArgument{group}, "Parameter group deleted");
}
void ngInitParameterGroupCommands() {
    initialization::Program::run<initialization::Table>();
}
} // namespace nijigenerate::commands::parameter::group
