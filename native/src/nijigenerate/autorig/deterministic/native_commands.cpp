#include "native_commands.hpp"
#include "detail/native_memory.hpp"
#include <nijigenerate/compat/throwable.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

std::shared_ptr<commands::CommandResult> command(std::shared_ptr<commands::Command> action,
        std::shared_ptr<commands::Context> context) {
    auto result = commands::ngRunCommand(action, context);
    if (!result || !result->succeeded)
        throw compat::ThrowableException(copyNativeMessage(result ? result->message :
            compat::RetainedString::fromStaticLiteral("Native rig command returned no result")),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),236);
    return result;
}

std::shared_ptr<nicxlive::core::nodes::Node> create(std::shared_ptr<commands::Command> action,
        std::shared_ptr<commands::Context> context) {
    using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
    auto result = std::dynamic_pointer_cast<commands::CreateResult<NodeRef>>(command(action, context));
    if (!result || result->created.size() != 1)
        throw compat::ThrowableException(
            copyNativeMessage(compat::RetainedString::fromStaticLiteral("Native rig creation returned an unexpected result")),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"),242);
    return result->created[0];
}

} // namespace nijigenerate::autorig::deterministic::native_detail
