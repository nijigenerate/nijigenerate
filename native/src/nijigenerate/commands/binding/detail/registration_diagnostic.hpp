#pragma once

#include "../binding_types.hpp"
#include "source_alias.hpp"
#include "constructor_body.hpp"
#include <string_view>

namespace nijigenerate::commands::binding::detail {

// Each declaration retains its own name even when enum values are aliases.
template<CommandLiteral Identifier> struct BindingRegistrationDeclaration;

#define NG_BINDING_DIAGNOSTIC(Member, Value, Spelling) \
    template<> struct BindingRegistrationDeclaration<#Member> { \
        using Declaration = typename decltype(bindingSourceDeclarationForName<#Member>())::type; \
        struct SourceAlias : BindingSourceAlias<Declaration::value, \
                Declaration::sourceIdentifier, Declaration::sourceStringOf> { \
            using ActualSourceDeclaration = Member##Command; \
            using Resolution = ResolvedBindingSourceClass< \
                BindingSourceAlias<Declaration::value, Declaration::sourceIdentifier, \
                    Declaration::sourceStringOf>::commandIdentifier, ActualSourceDeclaration>; \
        }; \
        static constexpr std::string_view value = bindingCommandRegistrationMessageFor<SourceAlias>.view(); \
        static constexpr std::string_view missingValue = SourceAlias::missingMessage.view(); \
        template<bool Compiles> static consteval bool require() { \
            static_assert(Compiles, "BindingCommand registration does not compile: " Spelling); \
            return Compiles; \
        } \
    };

NG_BINDING_ENUM_DECLARATIONS(NG_BINDING_DIAGNOSTIC)
#undef NG_BINDING_DIAGNOSTIC

template<BindingCommand Id>
using BindingNativeValueDeclaration = typename decltype(bindingSourceDeclarationForValue<Id>())::type;

// Compatibility for older native consumers that supplied only an enum value.
// This route is not used for Source declaration identity or generic lookup.
template<BindingCommand Id> struct BindingRegistrationDiagnostic :
    BindingRegistrationDeclaration<BindingNativeValueDeclaration<Id>::sourceIdentifier> {};

template<BindingCommand Id>
inline constexpr std::string_view bindingCommandRegistrationMessage =
    BindingRegistrationDiagnostic<Id>::value;

} // namespace nijigenerate::commands::binding::detail
