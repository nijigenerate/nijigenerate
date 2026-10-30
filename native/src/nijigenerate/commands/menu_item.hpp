#pragma once
#include "cmd.hpp"
#include "command.hpp"
#include "context.hpp"
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <imgui.h>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands {
namespace detail {
template<class Key, auto KeyOrigin, class Bindings> struct MenuCommandMap;

template<class Key, auto KeyOrigin, class... Bindings>
struct MenuCommandMap<Key, KeyOrigin, std::tuple<Bindings...>> {
    using type = MapByKeyTypeWithOrigin<Key, KeyOrigin, Bindings...>;
};

template<auto Id, auto SourceOrigin, auto ClassOrigin, auto KeyOrigin, class ReadKey, class... Args>
std::shared_ptr<Command> menuItemForSourceCommand(std::shared_ptr<Context>& ctx,
        bool selected, bool enabled, ReadKey& readKey, Args&... args) {
    using Key = typename decltype(ngSourceCommandIdKeyType<Id>())::type;
    using Binding = typename MenuCommandMap<Key, KeyOrigin, AllCommandMaps>::type;
    using ActualClass = typename decltype(commandSourceClassType<Id>())::type;
    if constexpr (std::is_void_v<ActualClass>) {
        static_assert(dependentMapError<decltype(Id)>,
            "Missing defining Source command symbol/class resolution for this id");
    } else if constexpr (std::same_as<ActualClass, std::nullptr_t>) {
        static_assert(ngRequireSourceCommandClassName<Id>(),
            "Missing defining Source command class-lookup diagnostic origin");
    } else {
        using Messages = CommandDispatchMessages<Id, ActualClass, ClassOrigin, KeyOrigin>;
        std::shared_ptr<Command> command;
        bool found = false;
        {
            // Read actual alias storage at the original resolution boundary.
            auto&& nativeId = std::invoke(readKey);
            auto& table = Binding::get();
            const auto at = table.find(nativeId);
            if (at != table.end()) { found = true; command = at->second; }
        }
        if (!found) {
            std::string message;
            appendReflectionText(message, "No registered command for id: ");
            appendReflectionText(message, Messages::keyExpression.view());
            appendReflectionText(message, " (key type: ");
            appendReflectionText(message, Messages::keyName);
            appendReflectionText(message, ")");
            const auto retainedMessage = retainReflectionText(std::move(message));
            throw compat::ThrowableException(retainedMessage,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/base.d"), 811);
        }
        if (!command) return {};
        const auto shortcut = core::shortcut::ngShortcutFor(command);
        const compat::SourceCString shortcutZ{compat::RetainedStringSlice(shortcut)};
        const auto label = command->label();
        const bool canRun = ngCommandAllowedInCurrentContext(command) && command->runnable(ctx);
        const auto translatedLabel = core::translation::ngTranslateCString(label.view());
        if (ImGui::MenuItem(translatedLabel.data(), shortcut.empty() ? nullptr : shortcutZ.data(),
                selected, enabled && canRun)) {
            // Re-read the alias and real TLS table after callbacks. Auto-ref
            // arguments are named lvalues at the original nested cmd call.
            auto&& nativeId = std::invoke(readKey);
            invokeSourceCommand<Id, SourceOrigin, ClassOrigin, KeyOrigin>(ctx, nativeId, args...);
        }
        return command;
    }
}
} // namespace detail

template<auto Id, auto SourceOrigin = nullptr, auto ClassOrigin = nullptr, auto KeyOrigin = nullptr>
    requires (!detail::SourceCommandAliasExpressionTraits<std::remove_cv_t<decltype(Id)>>::supplied)
std::shared_ptr<Command> ngMenuItemFor(std::shared_ptr<Context>& ctx,
        bool selected = false, bool enabled = true) {
    auto readKey = []() -> decltype(auto) { return ngSourceCommandIdValue<Id>(); };
    return detail::menuItemForSourceCommand<Id, SourceOrigin, ClassOrigin, KeyOrigin>(
        ctx, selected, enabled, readKey);
}

template<auto Id, auto SourceOrigin = nullptr, auto ClassOrigin = nullptr,
    auto KeyOrigin = nullptr, class... Args>
    requires (!detail::SourceCommandAliasExpressionTraits<std::remove_cv_t<decltype(Id)>>::supplied &&
        sizeof...(Args) > 0)
std::shared_ptr<Command> ngMenuItemFor(std::shared_ptr<Context>& ctx,
        bool selected, bool enabled, Args&&... args) {
    auto readKey = []() -> decltype(auto) { return ngSourceCommandIdValue<Id>(); };
    return detail::menuItemForSourceCommand<Id, SourceOrigin, ClassOrigin, KeyOrigin>(
        ctx, selected, enabled, readKey, args...);
}

// Runtime aliases keep their actual lvalue getter separate from Source naming
// metadata. The getter is read once at each original lookup, never as a key copy.
template<auto Id, auto SourceOrigin = nullptr, auto ClassOrigin = nullptr,
    auto KeyOrigin = nullptr, class ReadKey, class... Args>
    requires (detail::SourceCommandAliasExpressionTraits<std::remove_cv_t<decltype(Id)>>::supplied &&
        std::invocable<ReadKey&> && std::is_lvalue_reference_v<std::invoke_result_t<ReadKey&>>)
std::shared_ptr<Command> ngMenuItemFor(std::shared_ptr<Context>& ctx, ReadKey&& readKey,
        bool selected = false, bool enabled = true, Args&&... args) {
    return detail::menuItemForSourceCommand<Id, SourceOrigin, ClassOrigin, KeyOrigin>(
        ctx, selected, enabled, readKey, args...);
}

} // namespace nijigenerate::commands
