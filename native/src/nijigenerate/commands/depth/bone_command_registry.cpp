#include "bone_commands.hpp"
#include "detail/bone_action_scope.hpp"

namespace nijigenerate::commands::depth::bone {
namespace {
template<DepthBoneCommand id> requires(initialization::registrationSupported<id>())
void registerBoneCommand() {
    using Concrete = typename DepthBoneCommandType<id>::Type;
    auto command = ngCreateRegisteredCommand<Concrete>();
    commands[id] = command;
}
} // namespace

void ngInitDepthBoneCommands() {
    []<class... Item>(compat::TypeSequence<Item...>) {
        (registerBoneCommand<Item::value>(), ...);
    }(initialization::Keys{});
}

} // namespace nijigenerate::commands::depth::bone
