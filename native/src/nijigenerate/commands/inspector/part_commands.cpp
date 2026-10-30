#include "part_commands.hpp"
#include <compat/source_allocator.hpp>
#include <stdexcept>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitPartInspectorCommands() {
    {auto command=std::allocate_shared<PartTintCommand>(nicxlive::core::d_runtime::SourceAllocator<PartTintCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PartTint]=command;}
    {auto command=std::allocate_shared<PartScreenTintCommand>(nicxlive::core::d_runtime::SourceAllocator<PartScreenTintCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PartScreenTint]=command;}
    {auto command=std::allocate_shared<PartEmissionStrengthCommand>(nicxlive::core::d_runtime::SourceAllocator<PartEmissionStrengthCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PartEmissionStrength]=command;}
    {auto command=std::allocate_shared<PartBlendingModeCommand>(nicxlive::core::d_runtime::SourceAllocator<PartBlendingModeCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PartBlendingMode]=command;}
    {auto command=std::allocate_shared<PartOpacityCommand>(nicxlive::core::d_runtime::SourceAllocator<PartOpacityCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PartOpacity]=command;}
    {auto command=std::allocate_shared<PartMaskAlphaThresholdCommand>(nicxlive::core::d_runtime::SourceAllocator<PartMaskAlphaThresholdCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PartMaskAlphaThreshold]=command;}
    {auto command=std::allocate_shared<PartAutoResizedMeshCommand>(nicxlive::core::d_runtime::SourceAllocator<PartAutoResizedMeshCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PartAutoResizedMesh]=command;}
    {auto command=std::allocate_shared<TogglePartAutoResizedMeshCommand>(nicxlive::core::d_runtime::SourceAllocator<TogglePartAutoResizedMeshCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::TogglePartAutoResizedMesh]=command;}
}
}
namespace nijigenerate::panels::inspector::part {
std::shared_ptr<commands::CommandResult> ngRunPartPropertyCommand(commands::inspector::apply_node::InspectorNodeApplyCommand id,std::shared_ptr<commands::Context> context,PropertyValue value) {
    using namespace commands::inspector::apply_node;
    switch(id) {
#define NG_DISPATCH_PART(Name) \
    case InspectorNodeApplyCommand::Name: \
        return ::nijigenerate::commands::ngDispatchRegisteredCommand<Name##Command>( \
            ::nijigenerate::commands::inspector::apply_node::commands, id, context, \
            {"InspectorNodeApplyCommand." #Name, "InspectorNodeApplyCommand", #Name "Command"}, \
            std::get<std::reference_wrapper<const Name##Access::Value>>(value).get())
    NG_DISPATCH_PART(PartTint);
    NG_DISPATCH_PART(PartScreenTint);
    NG_DISPATCH_PART(PartEmissionStrength);
    NG_DISPATCH_PART(PartBlendingMode);
    NG_DISPATCH_PART(PartOpacity);
    NG_DISPATCH_PART(PartMaskAlphaThreshold);
    NG_DISPATCH_PART(PartAutoResizedMesh);
#undef NG_DISPATCH_PART
    default: throw std::logic_error("Native Part inspector caller supplied an untranslated property expression");
    }
}
std::string ngPartBlendName(BlendMode mode) {
    return composite::ngCompositeBlendName(mode);
}
}
