#pragma once

#include "apply_args.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <concepts>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::commands {

// Actual compile-time source expressions, supplied by each translated enum
// call. They describe the real class/map and never manufacture another map.
struct CommandDispatchSource {
    std::string_view keyExpression;
    std::string_view keyType;
    std::string_view concreteClass;
};

namespace detail {

template<class ActualClass, auto SourceOrigin = nullptr, auto ClassOrigin = nullptr,
    class Map, class Key, class ReadContext, class... Args>
std::shared_ptr<CommandResult> dispatchRegisteredCommand(Map& map, const Key& id,
        ReadContext&& readContext, CommandDispatchSource source, Args&&... args) {
    static_assert(std::derived_from<ActualClass, Command>);
    const auto found = map.find(id);
    if (found == map.end()) {
        compat::RetainedString message;
        try {
            const auto text = "No registered command for id: " + std::string(source.keyExpression) +
                " (key type: " + std::string(source.keyType) + ")";
            message = compat::RetainedString(std::string_view(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/package.d"), 358);
    }
    const auto base = found->second;
    if (!ngCommandAllowedInCurrentContext(base))
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral(
            "Command is not available in the current command scope"));
    const auto instance = std::dynamic_pointer_cast<ActualClass>(base);
    if (!instance) {
        compat::RetainedString message;
        try {
            const auto text = "Registered command instance type mismatch for id: " +
                std::string(source.keyExpression) + ", expected " + std::string(source.concreteClass);
            message = compat::RetainedString(std::string_view(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/package.d"), 376);
    }
    detail::applyArgs<ActualClass, SourceOrigin, ClassOrigin>(instance, std::forward<Args>(args)...);
    return ngRunCommand(instance, readContext());
}

} // namespace detail

template<class ActualClass, auto SourceOrigin = nullptr, auto ClassOrigin = nullptr,
    class Map, class Key, class... Args>
std::shared_ptr<CommandResult> ngDispatchRegisteredCommand(Map& map, const Key& id,
        std::shared_ptr<Context> context, CommandDispatchSource source, Args&&... args) {
    return detail::dispatchRegisteredCommand<ActualClass, SourceOrigin, ClassOrigin>(map, id,
        [&context]() -> const std::shared_ptr<Context>& { return context; },
        source, std::forward<Args>(args)...);
}

template<class ActualClass, auto SourceOrigin = nullptr, auto ClassOrigin = nullptr,
    class Map, class Key, class... Args>
std::shared_ptr<CommandResult> ngDispatchRegisteredCommandRef(Map& map, const Key& id,
        std::shared_ptr<Context>& context, CommandDispatchSource source, Args&&... args) {
    return detail::dispatchRegisteredCommand<ActualClass, SourceOrigin, ClassOrigin>(map, id,
        [&context]() -> const std::shared_ptr<Context>& { return context; },
        source, std::forward<Args>(args)...);
}

} // namespace nijigenerate::commands
