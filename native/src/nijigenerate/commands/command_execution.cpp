#include "command.hpp"
#include <nijigenerate/api/mcp/task/task.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

namespace nijigenerate::commands {
namespace {
// The original ordinary module boolean is thread-local, not process-shared.
thread_local bool autoRigEditorAction = false;
compat::RetainedString mainThreadEnforceMessage() {
    try {
        return compat::RetainedString(std::string_view("AutoRig editing must run on the main thread"));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}
}

void ngRunAutoRigEditorAction(compat::SharedDelegate<void()> action) {
    if (!core::thread_context::ngIsMainThread()) throw compat::ThrowableException(
        mainThreadEnforceMessage(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/base.d"), 252);
    const auto previous = autoRigEditorAction;
    autoRigEditorAction = true;
    struct Restore {
        bool previous;
        ~Restore() noexcept { autoRigEditorAction = previous; }
    } restore{previous};
    action();
}

bool ngCommandAllowedInCurrentContext(std::shared_ptr<Command> command) {
    if (!command) return false;
    const auto metadata = detail::lookupCommandMeta(*command);
    if (api::mcp::task::ngMcpExternalCommandsBlocked()) {
        if (metadata.presentationOnly) return true;
        if (!autoRigEditorAction) return false;
    }
    return compat::ngRequireObject(ngCurrentCommandScope())->permits(metadata);
}

std::shared_ptr<CommandResult> ngRunCommand(std::shared_ptr<Command> command, std::shared_ptr<Context> context) {
    if (!ngCommandAllowedInCurrentContext(command)) {
        return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("Command is not available in the current context"));
    }
    if (!command->runnable(context)) {
        return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("Command is not runnable in the current context"));
    }
    return command->run(context);
}

} // namespace nijigenerate::commands
