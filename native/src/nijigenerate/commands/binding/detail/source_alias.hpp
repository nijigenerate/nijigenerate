#pragma once

#include <nijigenerate/commands/fields.hpp>
#include "../binding_types.hpp"
#include <common/utils.hpp>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::binding::detail {

template<CommandLiteral Left, CommandLiteral Right>
consteval auto bindingSourceConcatenate() {
    char bytes[sizeof(Left.value) + sizeof(Right.value) - 1]{};
    std::size_t offset = 0;
    for (const char value : Left.view()) bytes[offset++] = value;
    for (const char value : Right.view()) bytes[offset++] = value;
    return CommandLiteral{bytes};
}

template<class Alias>
inline constexpr auto bindingCommandRegistrationMessageFor = bindingSourceConcatenate<
    "BindingCommand registration does not compile: ", Alias::sourceStringOf>();

// A native declaration descriptor, not an additional Source class or enum.
// Aliases with the same value retain their independent declaring identifiers.
// The spelling is supplied by the original declaration, not an enum-value lookup.
template<auto Value, CommandLiteral Identifier, CommandLiteral StringOf = Identifier>
struct BindingSourceAlias {
    inline static constexpr auto value = Value;
    inline static constexpr auto sourceIdentifier = Identifier;
    inline static constexpr auto sourceStringOf = StringOf;
    inline static constexpr auto commandIdentifier = bindingSourceConcatenate<Identifier, "Command">();
    inline static constexpr auto registrationMessage = bindingSourceConcatenate<
        "BindingCommand registration does not compile: ", StringOf>();
    inline static constexpr auto missingMessage = bindingSourceConcatenate<
        "BindingCommand was not registered: ", StringOf>();
};

template<class C> struct BindingSourceConstructor;

template<CommandLiteral Identifier> struct BindingSourceDeclaration {
    inline static constexpr std::string_view sourceScope = "nijigenerate.commands.binding.binding";
    inline static constexpr auto identifier = Identifier;
};

struct ConfirmedBindingSourceLookupFailureTag {};

// A declaring translation must establish the failed lookup against the original
// scope (missing or unusable declaration), before supplying this result.
// It is never the fallback for an unimplemented native lookup or constructor.
template<CommandLiteral Identifier>
struct ConfirmedBindingSourceLookupFailure : BindingSourceDeclaration<Identifier>,
        ConfirmedBindingSourceLookupFailureTag {};

// An actual resolved value/function is not an allocation type. Keep the real
// symbol in the native record; do not manufacture a stand-in Source class.
template<CommandLiteral Identifier, auto Symbol>
struct ResolvedBindingSourceValue : BindingSourceDeclaration<Identifier> {
    struct ConstructorBody {
        inline static constexpr auto actualSymbol = Symbol;
    };
};

template<CommandLiteral Identifier, class C, class BodyProducer = BindingSourceConstructor<C>>
struct ResolvedBindingSourceClass : BindingSourceDeclaration<Identifier> {
    using ActualType = C;
    using ConstructorBody = BodyProducer;
    template<class... Args, class Body = ConstructorBody>
    static auto construct(Args&&... args) -> decltype(Body::construct(std::forward<Args>(args)...)) {
        return Body::construct(std::forward<Args>(args)...);
    }
};

template<class Alias> consteval bool bindingCommandRegistrationCompiles() {
    // Absence of Resolution is a missing C++ translation, not a Source false.
    using Resolution = typename Alias::Resolution;
    constexpr auto generated = bindingSourceConcatenate<Alias::sourceIdentifier, "Command">();
    static_assert(Resolution::sourceScope == "nijigenerate.commands.binding.binding",
        "A binding alias must resolve in the original binding module scope");
    static_assert(Resolution::identifier.view() == generated.view(),
        "A binding alias must resolve its declaration identifier plus Command");
    if constexpr (std::is_base_of_v<ConfirmedBindingSourceLookupFailureTag, Resolution>) return false;
    else {
        // Deliberately outside the construction requirement: a malformed Source
        // comparison is a hard instantiation failure, not a false constructor.
        constexpr bool interpolation = [] {
            using Value = std::remove_cvref_t<decltype(Alias::value)>;
            if constexpr (std::is_same_v<Value, BindingCommand>) {
                return Alias::value == BindingCommand::SetInterpolation;
            } else if constexpr (std::is_arithmetic_v<Value>)
                return Alias::value == static_cast<std::int32_t>(BindingCommand::SetInterpolation);
            else if constexpr (Alias::value == BindingCommand::SetInterpolation) return true;
            else return false;
        }();
        static_assert(sizeof(typename Resolution::ConstructorBody) != 0,
            "The resolved Source constructor body needs its actual native producer");
        if constexpr (requires { typename Resolution::ActualType; }) {
            static_assert(std::is_same_v<typename Resolution::ActualType,
                typename Resolution::ConstructorBody::ActualType>,
                "The constructor producer must construct the actual resolved Source declaration");
        }
        if constexpr (interpolation)
            return requires { Resolution::construct(nicxlive::core::common::InterpolateMode::Linear); };
        else return requires { Resolution::construct(); };
    }
}

} // namespace nijigenerate::commands::binding::detail
