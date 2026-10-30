#include "camera_command.hpp"
#include <compat/source_allocator.hpp>
#include <stdexcept>
namespace nijigenerate::commands::inspector::apply_node {
void ngInitCameraViewportCommand() {
    auto command = std::allocate_shared<ViewportOriginCommand>(nicxlive::core::d_runtime::SourceAllocator<ViewportOriginCommand> {});
    ngRegisterCommandMeta(command);
    commands[InspectorNodeApplyCommand::ViewportOrigin] = command;
}
std::shared_ptr<CommandResult> ngRunCameraViewportCommand(std::shared_ptr<Context> context,
    nicxlive::core::math::Vec2 value) {
    auto found = commands.find(InspectorNodeApplyCommand::ViewportOrigin);
    if (found == commands.end()) throw std::runtime_error("No registered camera viewport command");
    auto base = found->second;
    if (!ngCommandAllowedInCurrentContext(base))
        return CommandResult::opCall(false, "Command is not available in the current command scope");
    auto command = std::dynamic_pointer_cast<ViewportOriginCommand>(base);
    if (!command) throw std::runtime_error("Registered camera viewport command type mismatch");
    command->value = value;
    return ngRunCommand(command, context);
}
}
