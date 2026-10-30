#pragma once

#include "all_command_maps.hpp"
#include "concrete_catalog.hpp"
#include "source_diagnostic_text.hpp"
#include "detail/cmd_map_adapters.hpp"
#include "detail/registered_dispatch.hpp"
#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace nijigenerate::commands::detail {

template<class Key, class Bindings, auto KeyOrigin = nullptr> struct CommandDispatchMap;

template<class Key, auto KeyOrigin, class... Bindings>
struct CommandDispatchMap<Key, std::tuple<Bindings...>, KeyOrigin> {
    using type = MapByKeyTypeWithOrigin<Key, KeyOrigin, Bindings...>;
};

template<auto Id> consteval auto commandSourceClassType() {
    using Origin = std::remove_cv_t<decltype(Id)>;
    using Key = typename decltype(ngSourceCommandIdKeyType<Id>())::type;
    if constexpr (sourceCommandCallOrigin<Origin>) {
        if constexpr (!std::is_void_v<typename Origin::CommandType>)
            return std::type_identity<typename Origin::CommandType>{};
        else if constexpr (SourceCommandIdExpressionTraits<Origin>::supplied &&
                requires { CommandMapKeyMetadata<Key>::members; }) {
            // Value-only catalogues cannot distinguish an arbitrary alias or
            // multiple declared symbols sharing one value. Demand its resolution.
            constexpr bool canonicalSymbol = [] {
                std::size_t values = 0;
                bool named = false;
                for (const auto& member : CommandMapKeyMetadata<Key>::members) {
                    if (member.value == ngSourceCommandIdValue<Id>()) {
                        ++values;
                        named = member.name == ngSourceCommandIdStringOf<Id>().view();
                    }
                }
                return values == 1 && named;
            }();
            if constexpr (!canonicalSymbol) return std::type_identity<void>{};
            else if constexpr (requires { typename CommandClassFor<ngSourceCommandIdValue<Id>()>::type; })
                return std::type_identity<typename CommandClassFor<ngSourceCommandIdValue<Id>()>::type>{};
            else return std::type_identity<void>{};
        } else return std::type_identity<void>{};
    } else if constexpr (requires { typename CommandClassFor<ngSourceCommandIdValue<Id>()>::type; })
        return std::type_identity<typename CommandClassFor<ngSourceCommandIdValue<Id>()>::type>{};
    else return std::type_identity<void>{};
}

template<auto Id, class ActualClass, auto ClassOrigin, auto KeyOrigin> struct CommandDispatchMessages {
    using Key = typename decltype(ngSourceCommandIdKeyType<Id>())::type;
    using Origin = std::remove_cv_t<decltype(Id)>;
    using Metadata = CommandMapKeyMetadata<Key>;
    inline static constexpr std::string_view keyName = [] {
        using Supplied = std::remove_cv_t<decltype(KeyOrigin)>;
        if constexpr (SourceCommandTypeOriginTraits<Supplied>::supplied) {
            if constexpr (HasLiteralCommandTypeStringOf<Key>)
                static_assert(Supplied::sourceStringOf.view() == ngCommandTypeStringOf<Key>().view(),
                    "Source command key origin disagrees with the selected key type");
            return Supplied::sourceStringOf.view();
        } else if constexpr (HasLiteralCommandTypeStringOf<Key>)
            return std::string_view(nicxlive::core::d_runtime::SourceTypeSpellings<Key>::unqualifiedElement);
        else return Metadata::typeName;
    }();
    inline static constexpr std::string_view memberName = [] {
        if constexpr (!SourceCommandAliasExpressionTraits<Origin>::supplied && requires { Metadata::members; })
            for (const auto& member : Metadata::members)
                if (member.value == ngSourceCommandIdValue<Id>()) return member.name;
        return std::string_view{};
    }();

    inline static constexpr auto keyExpression = [] {
        if constexpr (sourceCommandCallOrigin<Origin>)
            return ngSourceCommandIdStringOf<Id>();
        else {
            static_assert(!memberName.empty(), "Missing defining Source command id expression origin");
            SourceDiagnosticText<memberName.size()> result;
            for (std::size_t index = 0; index < memberName.size(); ++index)
                result.bytes[index] = memberName[index];
            return result;
        }
    }();
    inline static constexpr auto concreteClass = [] {
        using Supplied = std::remove_cv_t<decltype(ClassOrigin)>;
        if constexpr (SourceCommandTypeOriginTraits<Supplied>::supplied) {
            if constexpr (HasLiteralCommandClassStringOf<ActualClass>)
                static_assert(Supplied::sourceStringOf.view() == ngCommandClassStringOf<ActualClass>().view(),
                    "Source command class origin disagrees with the resolved class");
            return Supplied::sourceStringOf;
        } else if constexpr (HasLiteralCommandClassStringOf<ActualClass>)
            return ngCommandClassStringOf<ActualClass>();
        else if constexpr (std::same_as<ActualClass, node::node::AddNodeCommandT<true>>)
            return SourceDiagnosticText{"AddNodeCommandT!true"};
        else if constexpr (std::same_as<ActualClass, node::node::InsertNodeCommandT<true>>)
            return SourceDiagnosticText{"InsertNodeCommandT!true"};
        else if constexpr (std::same_as<ActualClass, node::node::ConvertToCommandT<true>>)
            return SourceDiagnosticText{"ConvertToCommandT!true"};
        else {
            if constexpr (sourceCommandCallOrigin<Origin>)
                static_assert(std::is_void_v<typename Origin::CommandType>,
                    "Missing defining Source stringof origin for the resolved command class");
            SourceDiagnosticText<memberName.size() + 7> result;
            constexpr std::string_view suffix = "Command";
            for (std::size_t index = 0; index < memberName.size(); ++index) result.bytes[index] = memberName[index];
            for (std::size_t index = 0; index < suffix.size(); ++index)
                result.bytes[memberName.size() + index] = suffix[index];
            return result;
        }
    }();
};

// Source cmd!(id) retains a reference to the caller's Context handle. In
// particular, argument assignment may rebind it before the final run call.
template<auto Id, auto ArgumentsOrigin, auto ClassOrigin, auto KeyOrigin, class NativeKey, class... Args>
std::shared_ptr<CommandResult> invokeSourceCommand(std::shared_ptr<Context>& context,
        const NativeKey& nativeId, Args&&... args) {
    using Key = typename decltype(ngSourceCommandIdKeyType<Id>())::type;
    using Binding = typename CommandDispatchMap<Key, AllCommandMaps, KeyOrigin>::type;
    using ActualClass = typename decltype(commandSourceClassType<Id>())::type;
    if constexpr (std::is_void_v<ActualClass>) {
        static_assert(dependentMapError<decltype(Id)>,
            "Missing defining Source command symbol/class resolution for this id");
    } else if constexpr (std::same_as<ActualClass, std::nullptr_t>) {
        // Only an actual defining lookup result may report a missing Source
        // class. An unprovided native mapping above is not that semantic result.
        static_assert(ngRequireSourceCommandClassName<Id>(),
            "Missing defining Source command class-lookup diagnostic origin");
    } else {
        static_assert(std::derived_from<ActualClass, Command>);
        using Messages = CommandDispatchMessages<Id, ActualClass, ClassOrigin, KeyOrigin>;

        const CommandDispatchSource source{
            Messages::keyExpression.view(),
            Messages::keyName,
            Messages::concreteClass.view()};
        // Source auto-ref arguments are named lvalues at this nested call. Do not
        // move their owners into fields before the remaining assignments finish.
        return ngDispatchRegisteredCommandRef<ActualClass, ArgumentsOrigin, ClassOrigin>(
            Binding::get(), nativeId, context, source, args...);
    }
}

} // namespace nijigenerate::commands::detail

namespace nijigenerate::commands {

// Keep the caller's Context reference and each named argument lvalue intact.
template<auto Id, auto ArgumentsOrigin = nullptr, auto ClassOrigin = nullptr,
    auto KeyOrigin = nullptr, class... Args>
    requires (!detail::SourceCommandAliasExpressionTraits<std::remove_cv_t<decltype(Id)>>::supplied)
std::shared_ptr<CommandResult> ngCmd(std::shared_ptr<Context>& context, Args&&... args) {
    return detail::invokeSourceCommand<Id, ArgumentsOrigin, ClassOrigin, KeyOrigin>(
        context, ngSourceCommandIdValue<Id>(), args...);
}

template<auto Id, auto ArgumentsOrigin = nullptr, auto ClassOrigin = nullptr,
    auto KeyOrigin = nullptr, class ReadKey, class... Args>
    requires (detail::SourceCommandAliasExpressionTraits<std::remove_cv_t<decltype(Id)>>::supplied &&
        std::invocable<ReadKey&> && std::is_lvalue_reference_v<std::invoke_result_t<ReadKey&>>)
std::shared_ptr<CommandResult> ngCmd(std::shared_ptr<Context>& context, ReadKey&& readKey, Args&&... args) {
    // Read the actual alias after all call arguments, before resolving its AA.
    auto&& nativeId = std::invoke(readKey);
    return detail::invokeSourceCommand<Id, ArgumentsOrigin, ClassOrigin, KeyOrigin>(context, nativeId, args...);
}

} // namespace nijigenerate::commands
