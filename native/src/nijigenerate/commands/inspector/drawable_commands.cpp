#include "drawable_commands.hpp"
#include <compat/source_allocator.hpp>
#include <stdexcept>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitDrawableOffsetCommands() {
    {
        auto x=std::allocate_shared<OffsetXCommand>(nicxlive::core::d_runtime::SourceAllocator<OffsetXCommand> {}); ngRegisterCommandMeta(x);
        commands[InspectorNodeApplyCommand::OffsetX]=x;
    }
    {
        auto y=std::allocate_shared<OffsetYCommand>(nicxlive::core::d_runtime::SourceAllocator<OffsetYCommand> {}); ngRegisterCommandMeta(y);
        commands[InspectorNodeApplyCommand::OffsetY]=y;
    }
}
std::shared_ptr<CommandResult> ngRunDrawableOffsetCommand(bool y,std::shared_ptr<Context> context,float value) {
    auto found=commands.find(y ? InspectorNodeApplyCommand::OffsetY : InspectorNodeApplyCommand::OffsetX);
    if(found==commands.end()) throw std::runtime_error("No registered drawable offset command");
    auto base=found->second;
    if(!ngCommandAllowedInCurrentContext(base))
        return CommandResult::opCall(false,"Command is not available in the current command scope");
    if(y) {
        auto command=std::dynamic_pointer_cast<OffsetYCommand>(base);
        if(!command) throw std::runtime_error("Registered offset Y command type mismatch");
        command->value=value;
    } else {
        auto command=std::dynamic_pointer_cast<OffsetXCommand>(base);
        if(!command) throw std::runtime_error("Registered offset X command type mismatch");
        command->value=value;
    }
    return ngRunCommand(base,context);
}
}
