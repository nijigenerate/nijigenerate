#pragma once

#include "binding_types.hpp"
#include "binding_commands.hpp"
#include "detail/canonical_constructor_programs.hpp"
#include "binding_table.hpp"
#include "detail/registration_diagnostic.hpp"
#include "detail/initialization.hpp"
#include <concepts>
#include <type_traits>

namespace nijigenerate::commands::binding {
// Original module-level constructor visibility guard, before initialization.
#define NG_BINDING_REQUIRE_REGISTRATION(Name, Value, Spelling) \
    static_assert(detail::BindingRegistrationDeclaration<#Name>:: \
        require<detail::bindingCommandRegistrationCompiles< \
            detail::BindingRegistrationDeclaration<#Name>::SourceAlias>()>());
NG_BINDING_ENUM_DECLARATIONS(NG_BINDING_REQUIRE_REGISTRATION)
#undef NG_BINDING_REQUIRE_REGISTRATION

void ngInitBindingCommands();
template<class T> requires (std::same_as<T, BindingCommand> && detail::Initialization::supported())
void ngInitCommands() { ngInitBindingCommands(); }
} // namespace nijigenerate::commands::binding

namespace nijigenerate::commands::detail {

template<> struct InitializerBody<binding::BindingCommand> {
    static consteval bool supported() { return binding::detail::Initialization::supported(); }
    template<class ActualKey = binding::BindingCommand>
        requires (std::same_as<ActualKey, binding::BindingCommand> && supported())
    static void invoke() { binding::ngInitCommands<ActualKey>(); }
};

} // namespace nijigenerate::commands::detail
