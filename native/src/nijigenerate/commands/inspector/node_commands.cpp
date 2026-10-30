#include "node_commands.hpp"
#include <compat/source_allocator.hpp>
#include <nijigenerate/commands/all_map_key_metadata.hpp>
#include <nijigenerate/compat/throwable.hpp>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitNodeInspectorCommands() {
    {auto command=std::allocate_shared<TranslationXCommand>(nicxlive::core::d_runtime::SourceAllocator<TranslationXCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::TranslationX]=command;}
    {auto command=std::allocate_shared<TranslationYCommand>(nicxlive::core::d_runtime::SourceAllocator<TranslationYCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::TranslationY]=command;}
    {auto command=std::allocate_shared<TranslationZCommand>(nicxlive::core::d_runtime::SourceAllocator<TranslationZCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::TranslationZ]=command;}
    {auto command=std::allocate_shared<RotationXCommand>(nicxlive::core::d_runtime::SourceAllocator<RotationXCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::RotationX]=command;}
    {auto command=std::allocate_shared<RotationYCommand>(nicxlive::core::d_runtime::SourceAllocator<RotationYCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::RotationY]=command;}
    {auto command=std::allocate_shared<RotationZCommand>(nicxlive::core::d_runtime::SourceAllocator<RotationZCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::RotationZ]=command;}
    {auto command=std::allocate_shared<ScaleXCommand>(nicxlive::core::d_runtime::SourceAllocator<ScaleXCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::ScaleX]=command;}
    {auto command=std::allocate_shared<ScaleYCommand>(nicxlive::core::d_runtime::SourceAllocator<ScaleYCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::ScaleY]=command;}
    {auto command=std::allocate_shared<PixelSnapCommand>(nicxlive::core::d_runtime::SourceAllocator<PixelSnapCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PixelSnap]=command;}
    {auto command=std::allocate_shared<TogglePixelSnapCommand>(nicxlive::core::d_runtime::SourceAllocator<TogglePixelSnapCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::TogglePixelSnap]=command;}
    {auto command=std::allocate_shared<ZSortCommand>(nicxlive::core::d_runtime::SourceAllocator<ZSortCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::ZSort]=command;}
    {auto command=std::allocate_shared<LockToRootCommand>(nicxlive::core::d_runtime::SourceAllocator<LockToRootCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::LockToRoot]=command;}
    {auto command=std::allocate_shared<ToggleLockToRootCommand>(nicxlive::core::d_runtime::SourceAllocator<ToggleLockToRootCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::ToggleLockToRoot]=command;}
    {auto command=std::allocate_shared<PinToMeshCommand>(nicxlive::core::d_runtime::SourceAllocator<PinToMeshCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::PinToMesh]=command;}
    {auto command=std::allocate_shared<TogglePinToMeshCommand>(nicxlive::core::d_runtime::SourceAllocator<TogglePinToMeshCommand> {});ngRegisterCommandMeta(command);commands[InspectorNodeApplyCommand::TogglePinToMesh]=command;}
}
}
namespace nijigenerate::panels::inspector::node {
namespace command_api=::nijigenerate::commands;
std::shared_ptr<commands::CommandResult> ngRunNodeInspectorPropertyCommand(commands::inspector::apply_node::InspectorNodeApplyCommand id,std::shared_ptr<commands::Context> context,PropertyValue value) {
    using namespace command_api::inspector::apply_node;
    const auto memberName=[&]() -> std::string_view {
        for(const auto member:command_api::detail::CommandMapKeyMetadata<InspectorNodeApplyCommand>::members)
            if(member.value==id)return member.name;
        return {};
    }();
    auto found=command_api::inspector::apply_node::commands.find(id);
    if(found==command_api::inspector::apply_node::commands.end())
        throw compat::ThrowableException(compat::RetainedString(
            "No registered command for id: "+std::string(memberName)+
            " (key type: InspectorNodeApplyCommand)"));
    auto base=found->second;
    if(!command_api::ngCommandAllowedInCurrentContext(base))
        return command_api::CommandResult::opCall(false,"Command is not available in the current command scope");
    auto assign=[&]<class C,class V>() {
        auto command=std::dynamic_pointer_cast<C>(base);
        if(!command)throw compat::ThrowableException(compat::RetainedString(
            "Registered command instance type mismatch for id: "+std::string(memberName)+
            ", expected "+std::string(memberName)+"Command"));
        command->value=std::get<std::reference_wrapper<const V>>(value).get();
    };
    switch(id) {
    case InspectorNodeApplyCommand::TranslationX:
        assign.template operator()<TranslationXCommand,TranslationXAccess::Value>();break;
    case InspectorNodeApplyCommand::TranslationY:
        assign.template operator()<TranslationYCommand,TranslationYAccess::Value>();break;
    case InspectorNodeApplyCommand::TranslationZ:
        assign.template operator()<TranslationZCommand,TranslationZAccess::Value>();break;
    case InspectorNodeApplyCommand::RotationX:
        assign.template operator()<RotationXCommand,RotationXAccess::Value>();break;
    case InspectorNodeApplyCommand::RotationY:
        assign.template operator()<RotationYCommand,RotationYAccess::Value>();break;
    case InspectorNodeApplyCommand::RotationZ:
        assign.template operator()<RotationZCommand,RotationZAccess::Value>();break;
    case InspectorNodeApplyCommand::ScaleX:
        assign.template operator()<ScaleXCommand,ScaleXAccess::Value>();break;
    case InspectorNodeApplyCommand::ScaleY:
        assign.template operator()<ScaleYCommand,ScaleYAccess::Value>();break;
    case InspectorNodeApplyCommand::PixelSnap:
        assign.template operator()<PixelSnapCommand,PixelSnapAccess::Value>();break;
    case InspectorNodeApplyCommand::ZSort:
        assign.template operator()<ZSortCommand,ZSortAccess::Value>();break;
    case InspectorNodeApplyCommand::LockToRoot:
        assign.template operator()<LockToRootCommand,LockToRootAccess::Value>();break;
    case InspectorNodeApplyCommand::PinToMesh:
        assign.template operator()<PinToMeshCommand,PinToMeshAccess::Value>();break;
    default:throw compat::ThrowableException("Invalid Node inspector property command");
    }
    return command_api::ngRunCommand(base,context);
}
}
