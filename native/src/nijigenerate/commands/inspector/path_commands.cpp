#include "path_commands.hpp"
#include <nijigenerate/commands/all_map_key_metadata.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <compat/source_allocator.hpp>
#include <stdexcept>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitPathInspectorCommands() {
    {
        auto command = std::allocate_shared<PathDeformDynamicCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformDynamicCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformDynamic] = command;
    }
    {
        auto command = std::allocate_shared<TogglePathDeformDynamicCommand>(nicxlive::core::d_runtime::SourceAllocator<TogglePathDeformDynamicCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::TogglePathDeformDynamic] = command;
    }
    {
        auto command = std::allocate_shared<PathDeformPhysicsEnabledCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformPhysicsEnabledCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformPhysicsEnabled] = command;
    }
    {
        auto command = std::allocate_shared<TogglePathDeformPhysicsEnabledCommand>(nicxlive::core::d_runtime::SourceAllocator<TogglePathDeformPhysicsEnabledCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::TogglePathDeformPhysicsEnabled] = command;
    }
    {
        auto command = std::allocate_shared<PathDeformCurveTypeCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformCurveTypeCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformCurveType] = command;
    }
    {
        auto command = std::allocate_shared<PathDeformGravityCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformGravityCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformGravity] = command;
    }
    {
        auto command = std::allocate_shared<PathDeformRestoreConstantCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformRestoreConstantCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformRestoreConstant] = command;
    }
    {
        auto command = std::allocate_shared<PathDeformDampingCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformDampingCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformDamping] = command;
    }
    {
        auto command = std::allocate_shared<PathDeformInputScaleCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformInputScaleCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformInputScale] = command;
    }
    {
        auto command = std::allocate_shared<PathDeformPropagateScaleCommand>(nicxlive::core::d_runtime::SourceAllocator<PathDeformPropagateScaleCommand> {});
        ngRegisterCommandMeta(command);
        commands[InspectorNodeApplyCommand::PathDeformPropagateScale] = command;
    }
}
}
namespace nijigenerate::panels::inspector::pathdeform {
namespace command_api = ::nijigenerate::commands;
void ngRunPathPropertyCommand(commands::inspector::apply_node::InspectorNodeApplyCommand id,
    std::shared_ptr<Inspector> inspector,Selection<Node> nodes,PropertyValue value) {
    using namespace command_api::inspector::apply_node;
    auto context=std::allocate_shared<command_api::Context>(
        nicxlive::core::d_runtime::SourceAllocator<command_api::Context>{});
    context->inspectors(command_api::Context::Inspectors{std::move(inspector)});
    context->nodes(static_cast<command_api::Context::Nodes>(nodes));
    ngRunPathPropertyCommand(id,std::move(context),value);
}
void ngRunPathPropertyCommand(commands::inspector::apply_node::InspectorNodeApplyCommand id,
    std::shared_ptr<commands::Context> context,PropertyValue value) {
    using namespace command_api::inspector::apply_node;
    switch(id) {
#define NG_DISPATCH_PATH(Name) \
    case InspectorNodeApplyCommand::Name: \
        (void)command_api::ngDispatchRegisteredCommand<Name##Command>( \
            command_api::inspector::apply_node::commands, id, context, \
            {"InspectorNodeApplyCommand." #Name, "InspectorNodeApplyCommand", #Name "Command"}, \
            std::get<std::reference_wrapper<const Name##Access::Value>>(value).get()); \
        return
    NG_DISPATCH_PATH(PathDeformDynamic);
    NG_DISPATCH_PATH(PathDeformPhysicsEnabled);
    NG_DISPATCH_PATH(PathDeformCurveType);
    NG_DISPATCH_PATH(PathDeformGravity);
    NG_DISPATCH_PATH(PathDeformRestoreConstant);
    NG_DISPATCH_PATH(PathDeformDamping);
    NG_DISPATCH_PATH(PathDeformInputScale);
    NG_DISPATCH_PATH(PathDeformPropagateScale);
#undef NG_DISPATCH_PATH
    default:throw std::logic_error("Native Path inspector caller supplied an untranslated property expression");
    }
}
}
