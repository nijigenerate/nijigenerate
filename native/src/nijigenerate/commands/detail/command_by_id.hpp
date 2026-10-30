#pragma once

#include <type_traits>
#include <nijigenerate/commands/source_diagnostic_text.hpp>

namespace nijigenerate::commands::detail {

template<auto Id, class C> constexpr bool commandTypeIdMatches() {
    if constexpr (requires { C::id; }) {
        constexpr bool needsInstance = [] {
            if constexpr (requires { &C::id; }) return std::is_member_pointer_v<decltype(&C::id)>;
            else return false;
        }();
        // A class-qualified Source expression has no instance for a member slot.
        if constexpr (!needsInstance) {
            // Native constexpr storage adds const to Source manifest enum values.
            if constexpr (std::is_same_v<std::remove_cv_t<decltype(C::id)>,
                                        typename decltype(ngSourceCommandIdKeyType<Id>())::type>) {
                using Origin = std::remove_cv_t<decltype(Id)>;
                if constexpr (SourceCommandAliasExpressionTraits<Origin>::supplied) {
                    static_assert(!SourceCommandAliasExpressionTraits<Origin>::supplied,
                        "A defining constant Source alias value is required for static command-id comparison");
                } else {
                    constexpr auto value = ngSourceCommandIdValue<Id>();
                    return C::id == value;
                }
            }
        }
    }
    return false;
}

template<auto Id, class... Commands> struct CommandByIdImpl;
template<auto Id> struct CommandByIdImpl<Id> {
    static consteval auto sourceFailureText() {
        return ngRegisteredSourceCommandFailureText<Id>();
    }
    static_assert(ngRequireRegisteredSourceCommand<Id>(),
        "Missing defining Source alias-expression diagnostic producer for exhausted command id");
};

template<bool Matches, auto Id, class First, class... Rest> struct CommandByIdStep;

template<auto Id, class First, class... Rest>
struct CommandByIdStep<true, Id, First, Rest...> {
    using type = First;
};

template<auto Id, class First, class... Rest>
struct CommandByIdStep<false, Id, First, Rest...> : CommandByIdImpl<Id, Rest...> {};

template<auto Id, class First, class... Rest>
struct CommandByIdImpl<Id, First, Rest...>
    : CommandByIdStep<commandTypeIdMatches<Id, First>(), Id, First, Rest...> {};

} // namespace nijigenerate::commands::detail
