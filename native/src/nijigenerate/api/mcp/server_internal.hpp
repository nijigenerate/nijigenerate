#pragma once

#include "nijigenerate/commands/command.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <compat/source_allocator.hpp>
#include <concepts>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::api::mcp::server::detail {

// Only builtin text construction belongs here; evaluate provider calls first.
template<class Build> auto allocateMcpText(Build&& build) {
    try {
        return std::forward<Build>(build)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T, class... Args>
std::shared_ptr<T> createMcpObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}

template<class C>
    requires (std::derived_from<C, commands::Command> &&
        std::convertible_to<std::shared_ptr<C>, std::shared_ptr<commands::Command>>)
std::shared_ptr<commands::CommandResult> _mcpRunCommandInstance(
    std::shared_ptr<C> instance, std::shared_ptr<commands::Context> context, compat::RetainedStringSlice) {
    return commands::ngRunCommand(instance, context);
}

} // namespace nijigenerate::api::mcp::server::detail
