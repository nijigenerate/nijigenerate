#include "simplephysics_commands.hpp"
#include <compat/source_allocator.hpp>
#include <nijigenerate/commands/node/simplephysics.hpp>
#include <nijigenerate/commands/node/simplephysics_parameter_commands.hpp>
#include <nijigenerate/commands/all_maps.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <stdexcept>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitSimplePhysicsInspectorCommands() {
    { auto value=std::allocate_shared<SimplePhysicsModelTypeCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsModelTypeCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsModelType]=value; }
    { auto value=std::allocate_shared<SimplePhysicsMapModeCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsMapModeCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsMapMode]=value; }
    { auto value=std::allocate_shared<SimplePhysicsLocalOnlyCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsLocalOnlyCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsLocalOnly]=value; }
    { auto value=std::allocate_shared<ToggleSimplePhysicsLocalOnlyCommand>(nicxlive::core::d_runtime::SourceAllocator<ToggleSimplePhysicsLocalOnlyCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::ToggleSimplePhysicsLocalOnly]=value; }
    { auto value=std::allocate_shared<SimplePhysicsGravityCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsGravityCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsGravity]=value; }
    { auto value=std::allocate_shared<SimplePhysicsLengthCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsLengthCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsLength]=value; }
    { auto value=std::allocate_shared<SimplePhysicsFrequencyCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsFrequencyCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsFrequency]=value; }
    { auto value=std::allocate_shared<SimplePhysicsAngleDampingCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsAngleDampingCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsAngleDamping]=value; }
    { auto value=std::allocate_shared<SimplePhysicsLengthDampingCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsLengthDampingCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsLengthDamping]=value; }
    { auto value=std::allocate_shared<SimplePhysicsOutputScaleXCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsOutputScaleXCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsOutputScaleX]=value; }
    { auto value=std::allocate_shared<SimplePhysicsOutputScaleYCommand>(nicxlive::core::d_runtime::SourceAllocator<SimplePhysicsOutputScaleYCommand> {}); ngRegisterCommandMeta(value); commands[InspectorNodeApplyCommand::SimplePhysicsOutputScaleY]=value; }
}
}
namespace nijigenerate::panels::inspector::simplephysics {
std::shared_ptr<commands::CommandResult> ngRunSimplePhysicsPropertyCommand(
    commands::inspector::apply_node::InspectorNodeApplyCommand id,std::shared_ptr<commands::Context> context,PropertyValue value) {
    using namespace commands::inspector::apply_node;
    switch(id) {
#define NG_DISPATCH_SIMPLE_PHYSICS(Name) \
    case InspectorNodeApplyCommand::Name: \
        return ::nijigenerate::commands::ngDispatchRegisteredCommand<Name##Command>( \
            ::nijigenerate::commands::inspector::apply_node::commands, id, context, \
            {"InspectorNodeApplyCommand." #Name, "InspectorNodeApplyCommand", #Name "Command"}, \
            std::get<std::reference_wrapper<const Name##Access::Value>>(value).get())
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsModelType);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsMapMode);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsLocalOnly);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsGravity);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsLength);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsFrequency);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsAngleDamping);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsLengthDamping);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsOutputScaleX);
    NG_DISPATCH_SIMPLE_PHYSICS(SimplePhysicsOutputScaleY);
#undef NG_DISPATCH_SIMPLE_PHYSICS
    default: throw std::logic_error("Native SimplePhysics caller supplied an untranslated property expression");
    }
}
void ngRunSimplePhysicsParameterCommand(std::shared_ptr<commands::Context> context,
    const std::shared_ptr<Parameter>& parameter,bool clear) {
    namespace source = commands::node::simplephysics;
    using Id = source::NodeSimplePhysicsCommand;
    if(clear) {
        (void)commands::ngDispatchRegisteredCommand<source::ClearSimplePhysicsParameterCommand>(
            source::commands, Id::ClearSimplePhysicsParameter, context,
            {"NodeSimplePhysicsCommand.ClearSimplePhysicsParameter", "NodeSimplePhysicsCommand",
                "ClearSimplePhysicsParameterCommand"});
    } else {
        (void)commands::ngDispatchRegisteredCommand<source::SetSimplePhysicsParameterCommand>(
            source::commands, Id::SetSimplePhysicsParameter, context,
            {"NodeSimplePhysicsCommand.SetSimplePhysicsParameter", "NodeSimplePhysicsCommand",
                "SetSimplePhysicsParameterCommand"}, parameter);
    }
}
std::string ngPhysicsModelName(PhysicsModel value) {
    const std::array arguments{core::translation::ngFormatArgument(value)};
    return core::translation::ngFormatTyped("%s", arguments);
}
std::string ngPhysicsMapModeName(ParamMapMode value) {
    const std::array arguments{core::translation::ngFormatArgument(value)};
    return core::translation::ngFormatTyped("%s", arguments);
}
}
