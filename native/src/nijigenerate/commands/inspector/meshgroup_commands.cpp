#include "meshgroup_commands.hpp"
#include <compat/source_allocator.hpp>
#include <stdexcept>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitMeshGroupPropertyCommands() {
    {
        auto x=std::allocate_shared<MeshGroupDynamicCommand>(nicxlive::core::d_runtime::SourceAllocator<MeshGroupDynamicCommand> {}); ngRegisterCommandMeta(x);
        commands[InspectorNodeApplyCommand::MeshGroupDynamic]=x;
    }
    {
        auto toggleDynamic=std::allocate_shared<ToggleMeshGroupDynamicCommand>(nicxlive::core::d_runtime::SourceAllocator<ToggleMeshGroupDynamicCommand> {}); ngRegisterCommandMeta(toggleDynamic);
        commands[InspectorNodeApplyCommand::ToggleMeshGroupDynamic]=toggleDynamic;
    }
    {
        auto y=std::allocate_shared<MeshGroupTranslateChildrenCommand>(nicxlive::core::d_runtime::SourceAllocator<MeshGroupTranslateChildrenCommand> {}); ngRegisterCommandMeta(y);
        commands[InspectorNodeApplyCommand::MeshGroupTranslateChildren]=y;
    }
    {
        auto toggleChildren=std::allocate_shared<ToggleMeshGroupTranslateChildrenCommand>(nicxlive::core::d_runtime::SourceAllocator<ToggleMeshGroupTranslateChildrenCommand> {}); ngRegisterCommandMeta(toggleChildren);
        commands[InspectorNodeApplyCommand::ToggleMeshGroupTranslateChildren]=toggleChildren;
    }
}
std::shared_ptr<CommandResult> ngRunMeshGroupPropertyCommand(bool y,std::shared_ptr<Context> context,bool value) {
    auto found=commands.find(y ? InspectorNodeApplyCommand::MeshGroupTranslateChildren : InspectorNodeApplyCommand::MeshGroupDynamic);
    if(found==commands.end()) throw std::runtime_error("No registered mesh group property command");
    auto base=found->second;
    if(!ngCommandAllowedInCurrentContext(base))
        return CommandResult::opCall(false,"Command is not available in the current command scope");
    if(y) {
        auto command=std::dynamic_pointer_cast<MeshGroupTranslateChildrenCommand>(base);
        if(!command) throw std::runtime_error("Registered translate children command type mismatch");
        command->value=value;
    } else {
        auto command=std::dynamic_pointer_cast<MeshGroupDynamicCommand>(base);
        if(!command) throw std::runtime_error("Registered dynamic mesh group command type mismatch");
        command->value=value;
    }
    return ngRunCommand(base,context);
}
}

