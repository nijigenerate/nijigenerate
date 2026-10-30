#include "griddeform_commands.hpp"
#include <compat/source_allocator.hpp>
#include <stdexcept>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitGridInspectorCommands() {
    {
        auto apply=std::allocate_shared<GridDeformDynamicCommand>(nicxlive::core::d_runtime::SourceAllocator<GridDeformDynamicCommand> {});ngRegisterCommandMeta(apply);
        commands[InspectorNodeApplyCommand::GridDeformDynamic]=apply;
    }
    {
        auto toggle=std::allocate_shared<ToggleGridDeformDynamicCommand>(nicxlive::core::d_runtime::SourceAllocator<ToggleGridDeformDynamicCommand> {});ngRegisterCommandMeta(toggle);
        commands[InspectorNodeApplyCommand::ToggleGridDeformDynamic]=toggle;
    }
}
}
namespace nijigenerate::panels::inspector::griddeform {
void ngRunGridDynamicCommand(std::shared_ptr<Inspector> inspector,Selection<Node> nodes,bool value) {
    using namespace commands::inspector::apply_node;
    auto found=commands::inspector::apply_node::commands.find(InspectorNodeApplyCommand::GridDeformDynamic);
    if(found==commands::inspector::apply_node::commands.end())throw std::runtime_error("No registered Grid inspector command");
    auto command=std::dynamic_pointer_cast<GridDeformDynamicCommand>(found->second);
    if(!command)throw std::runtime_error("Registered Grid inspector command type mismatch");
    if(!commands::ngCommandAllowedInCurrentContext(command))return;
    auto context=std::make_shared<commands::Context>();
    context->inspectors(commands::Context::Inspectors{std::move(inspector)});
    context->nodes(static_cast<commands::Context::Nodes>(nodes));
    command->value=value;commands::ngRunCommand(command,context);
}
}
