#pragma once

#include <nijigenerate/commands/fields.hpp>
#include <cstdint>
#include <tuple>
#include <type_traits>

// One declaring source for the enum and its declaration identities. The
// explicit runtime registration statements remain a separate Source program.
#define NG_BINDING_ENUM_DECLARATIONS(X) \
    X(UnsetKeyFrame, 0, "UnsetKeyFrame") \
    X(SetKeyFrame, 1, "SetKeyFrame") \
    X(ResetKeyFrame, 2, "ResetKeyFrame") \
    X(InvertKeyFrame, 3, "InvertKeyFrame") \
    X(MirrorKeyFrameHorizontally, 4, "MirrorKeyFrameHorizontally") \
    X(MirrorKeyFrameVertically, 5, "MirrorKeyFrameVertically") \
    X(FlipDeform, 6, "FlipDeform") \
    X(SymmetrizeDeform, 7, "SymmetrizeDeform") \
    X(SetFromHorizontalMirror, 8, "SetFromHorizontalMirror") \
    X(SetFromVerticalMirror, 9, "SetFromVerticalMirror") \
    X(SetFromDiagonalMirror, 10, "SetFromDiagonalMirror") \
    X(SetFrom1DMirror, 11, "SetFrom1DMirror") \
    X(CopyBinding, 12, "CopyBinding") \
    X(PasteBinding, 13, "PasteBinding") \
    X(RemoveBinding, 14, "RemoveBinding") \
    X(SetInterpolation, 15, "SetInterpolation")

namespace nijigenerate::commands::binding {
enum class BindingCommand : std::int32_t {
#define NG_BINDING_ENUM_VALUE(Name, Value, Spelling) Name = Value,
    NG_BINDING_ENUM_DECLARATIONS(NG_BINDING_ENUM_VALUE)
#undef NG_BINDING_ENUM_VALUE
};

namespace detail {
template<BindingCommand Value, CommandLiteral Identifier, CommandLiteral StringOf>
struct BindingCommandSourceDeclaration {
    inline static constexpr auto value = Value;
    inline static constexpr auto sourceIdentifier = Identifier;
    inline static constexpr auto sourceStringOf = StringOf;
};

inline constexpr auto bindingCommandSourceDeclarations = std::tuple{
#define NG_BINDING_ENUM_DECLARATION(Name, Value, Spelling) \
    BindingCommandSourceDeclaration<BindingCommand::Name, #Name, Spelling>{},
    NG_BINDING_ENUM_DECLARATIONS(NG_BINDING_ENUM_DECLARATION)
#undef NG_BINDING_ENUM_DECLARATION
};

template<CommandLiteral Identifier, std::size_t Index = 0>
consteval auto bindingSourceDeclarationForName() {
    using Declarations = decltype(bindingCommandSourceDeclarations);
    static_assert(Index < std::tuple_size_v<Declarations>, "Missing native enum declaration identity");
    if constexpr (Index < std::tuple_size_v<Declarations>) {
        using Declaration = std::remove_cv_t<std::tuple_element_t<Index, Declarations>>;
        if constexpr (Declaration::sourceIdentifier.view() == Identifier.view())
            return std::type_identity<Declaration>{};
        else return bindingSourceDeclarationForName<Identifier, Index + 1>();
    }
}

// Native value-only compatibility cannot recover an alias declaration. Source
// guards and registrations use the name-keyed declaration path above instead.
template<BindingCommand Value, std::size_t Index = 0>
consteval auto bindingSourceDeclarationForValue() {
    using Declarations = decltype(bindingCommandSourceDeclarations);
    static_assert(Index < std::tuple_size_v<Declarations>, "No declaration for the native enum value");
    if constexpr (Index < std::tuple_size_v<Declarations>) {
        using Declaration = std::remove_cv_t<std::tuple_element_t<Index, Declarations>>;
        if constexpr (Declaration::value == Value) return std::type_identity<Declaration>{};
        else return bindingSourceDeclarationForValue<Value, Index + 1>();
    }
}
} // namespace detail
} // namespace nijigenerate::commands::binding
