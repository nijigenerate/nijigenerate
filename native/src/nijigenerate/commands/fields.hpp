#pragma once

#include "base.hpp"
#include <common/nullable_string.hpp>
#include <common/resource_key_map.hpp>
#include <common/shared_array.hpp>
#include <math/types.hpp>
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::core::settings { class SettingsJsonValue; class SettingsString; }
namespace nicxlive::core::nodes { class SourcePhysicsModel; class SourceParamMapMode; }

namespace nijigenerate::commands {

// A defining type supplies its actual Source assignment body here. An absent
// mapping is incomplete, rather than a false implicit-conversion decision.
template<class To, class From, class Enable = void> struct SourceCommandAssignment;
template<class To, class From, class Enable = void> struct SourceCommandInitialization;
template<class To, class From, class Enable = void> struct SourceCommandParameterCopy;
template<class To, class From, class Enable = void> struct SourceCommandImplicitConversion;

template<class To, class From> consteval bool ngCommandAssignmentSupported();
template<class To, class From> consteval bool ngCommandInitializationSupported();

// A defining Source alias-this mapping can select a data member or getter. The
// auto return type instantiates that actual member expression, without invoking
// it during _applyArgsFor's compile-time conversion check.
template<auto Member> struct SourceCommandAliasThisConversion {
    template<class From> static decltype(auto) convert(From&& value) {
        return std::invoke(Member, std::forward<From>(value));
    }
};

template<auto Member> struct SourceCommandOpAssign {
    template<class To, class From> static auto assign(To& target, From&& value)
        -> decltype(std::invoke(Member, target, std::forward<From>(value))) {
        return std::invoke(Member, target, std::forward<From>(value));
    }
};

template<auto Member> struct SourceCommandAliasThisTargetAssignment {
    template<class To, class From>
        requires (std::invocable<decltype(Member), To&> &&
            ngCommandAssignmentSupported<
                std::remove_reference_t<std::invoke_result_t<decltype(Member), To&>>, From&&>())
    static void assign(To& target, From&& value) {
        auto&& actual = std::invoke(Member, target);
        SourceCommandAssignment<std::remove_reference_t<decltype(actual)>, From&&>::assign(
            actual, std::forward<From>(value));
    }
};

template<auto Member> struct SourceCommandAliasThisValueAssignment {
    template<class To, class From>
        requires (std::invocable<decltype(Member), From&&> &&
            ngCommandAssignmentSupported<To, std::invoke_result_t<decltype(Member), From&&>&&>())
    static void assign(To& target, From&& value) {
        auto&& actual = std::invoke(Member, std::forward<From>(value));
        SourceCommandAssignment<To, decltype(actual)>::assign(
            target, std::forward<decltype(actual)>(actual));
    }
};

template<std::size_t N> struct CommandLiteral {
    char value[N];
    constexpr CommandLiteral(const char (&text)[N]) {
        for (std::size_t i = 0; i < N; ++i) value[i] = text[i];
    }
    constexpr std::string_view view() const { return {value, N - 1}; }
};

template<class T> struct CommandMember;
template<class T, class Owner> struct CommandMember<T Owner::*> {
    using Type = T;
    using Class = Owner;
};

// The pointer is the only value accessor. Fields remain actual named members of
// the storage class; metadata never introduces a second tuple of argument values.
template<auto Member, CommandLiteral Name, CommandLiteral Description, CommandLiteral DTypeName,
    bool Named = true, bool Hidden = false, CommandJsonSchema Schema = CommandJsonSchema::unspecified>
struct FieldDescriptor {
    static_assert(std::is_member_object_pointer_v<decltype(Member)>);
    using Type = typename CommandMember<decltype(Member)>::Type;
    using Owner = typename CommandMember<decltype(Member)>::Class;
    static constexpr auto member = Member;
    static constexpr auto fieldName = Name.view();
    static constexpr auto fieldDesc = Description.view();
    static constexpr auto typeName = DTypeName.view();
    static constexpr bool named = Named;
    static constexpr bool hidden = Hidden;
    static constexpr auto jsonSchema = Schema;
    static decltype(auto) get(Owner& owner) { return (owner.*Member); }
    static decltype(auto) get(const Owner& owner) { return (owner.*Member); }
};

// Specialize for each storage type, in the original ExCommand parameter order.
// Storage is a base class of the eventual ExCommand implementation, so its fields
// are initialized before that implementation assigns constructor arguments.
template<class Storage> struct CommandFields;

namespace detail {
template<class First, class Second> constexpr bool distinctCommandFields() {
    if (First::fieldName == Second::fieldName) return false;
    // Base and derived member-pointer types can still identify the same field.
    if constexpr (requires { First::member == Second::member; })
        return First::member != Second::member;
    else return true;
}

template<class Descriptors, std::size_t I, std::size_t... J>
constexpr bool distinctCommandFieldRow(std::index_sequence<J...>) {
    return ((I >= J || distinctCommandFields<std::tuple_element_t<I, Descriptors>,
        std::tuple_element_t<J, Descriptors>>()) && ...);
}

template<class Storage> constexpr bool validCommandFields() {
    using Descriptors = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    return []<std::size_t... I>(std::index_sequence<I...> indices) {
        return ((std::is_convertible_v<Storage&, typename std::tuple_element_t<I, Descriptors>::Owner&> &&
            !std::tuple_element_t<I, Descriptors>::fieldName.empty()) && ...) &&
            (distinctCommandFieldRow<Descriptors, I>(indices) && ...);
    }(std::make_index_sequence<std::tuple_size_v<Descriptors>>{});
}
} // namespace detail

struct CommandArgMeta {
    compat::RetainedString typeName;
    compat::RetainedString fieldName;
    compat::RetainedString fieldDesc;
};

template<class Storage> compat::Slice<CommandArgMeta> ngReflectCommandArgMeta() {
    static_assert(detail::validCommandFields<Storage>(),
        "CommandFields must map distinct nonempty field names to distinct storage members");
    compat::Slice<CommandArgMeta> result;
    std::apply([&](const auto&... descriptor) {
        ([&] {
            using F = std::remove_cvref_t<decltype(descriptor)>;
            static_assert(std::is_base_of_v<typename F::Owner, Storage>);
            // D reflectArgMeta includes hidden TW fields, but excludes bare types.
            if constexpr (F::named) result.append({compat::RetainedString::fromStaticView(F::typeName),
                compat::RetainedString::fromStaticView(F::fieldName),
                compat::RetainedString::fromStaticView(F::fieldDesc)});
        }(), ...);
    }, CommandFields<Storage>::descriptors);
    return result;
}

namespace detail {
template<class T> struct CommandSharedPointer : std::false_type {};
template<class T> struct CommandSharedPointer<std::shared_ptr<T>> : std::true_type {};

template<class T, bool = std::is_enum_v<T>> struct CommandNumericSource { using Type = T; };
template<class T> struct CommandNumericSource<T, true> { using Type = std::underlying_type_t<T>; };

template<class T> inline constexpr bool commandStringDescriptor =
    std::same_as<T, compat::RetainedString> ||
    std::derived_from<T, compat::RetainedStringSlice> ||
    std::same_as<T, nicxlive::core::common::NullableString>;

template<class T> inline constexpr bool commandBuiltinValue =
    std::is_arithmetic_v<T> || std::is_enum_v<T> || CommandSharedPointer<T>::value ||
    commandStringDescriptor<T> || std::same_as<T, std::string> || std::same_as<T, std::nullptr_t>;

template<class T> struct CommandInitializationDescriptor : std::false_type {};
template<class T> struct CommandInitializationDescriptor<compat::Slice<T>> : std::true_type {};
template<class T> struct CommandInitializationDescriptor<nicxlive::core::common::SharedArray<T>>
    : std::true_type {};
template<class Key, class Value, class Hash, class Equal>
struct CommandInitializationDescriptor<nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal>>
    : std::true_type {};
template<class Result, class... Args>
struct CommandInitializationDescriptor<compat::SharedDelegate<Result(Args...)>> : std::true_type {};

// These actual carriers have empty/static-only default backing and real Source
// copy construction. Unknown structures require their defining initializer.
template<class T> struct CommandInitializationValue : std::bool_constant<
    std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_pointer_v<T> || CommandSharedPointer<T>::value ||
    std::same_as<T, compat::RetainedString> || std::same_as<T, compat::RetainedStringSlice> ||
    std::same_as<T, core::settings::SettingsString> ||
    std::same_as<T, nicxlive::core::common::NullableString> || std::same_as<T, std::nullptr_t> ||
    CommandInitializationDescriptor<T>::value ||
    std::same_as<T, core::settings::SettingsJsonValue> ||
    std::same_as<T, nicxlive::core::nodes::SourcePhysicsModel> ||
    std::same_as<T, nicxlive::core::nodes::SourceParamMapMode> ||
    std::same_as<T, nicxlive::core::math::Vec2> ||
    std::same_as<T, nicxlive::core::math::Vec3> ||
    std::same_as<T, nicxlive::core::math::Vec4> ||
    std::same_as<T, nicxlive::core::math::Vec2i> ||
    std::same_as<T, nicxlive::core::math::Vec4i>> {};
template<class T, std::size_t N> struct CommandInitializationValue<std::array<T, N>>
    : std::bool_constant<N == 0 || CommandInitializationValue<std::remove_cv_t<T>>::value> {};

template<class To, class From> constexpr bool acceptsBuiltinCommandValue();

template<class To, class From> constexpr bool acceptsBuiltinCommandInitialization() {
    using Target = std::remove_cvref_t<To>;
    using Source = std::remove_cvref_t<From>;
    if constexpr (std::is_const_v<To> || std::is_volatile_v<To>) return false;
    else if constexpr (std::same_as<Target, Source>) return std::is_assignable_v<To&, From>;
    else if constexpr (std::same_as<Source, std::nullptr_t> &&
            (CommandInitializationDescriptor<Target>::value || std::is_pointer_v<Target>)) return true;
    else if constexpr (commandBuiltinValue<Target> && commandBuiltinValue<Source>)
        return acceptsBuiltinCommandValue<std::remove_const_t<To>, From>();
    else return false;
}

// Dynamic-value assignment rules measured with LDC, not C++ convertibility.
// Constant-expression narrowing and user-defined D opAssign/alias-this require
// their own source contracts; they are deliberately not inferred here.
template<class To, class From> constexpr bool acceptsBuiltinCommandValue() {
    using Target = std::remove_cvref_t<To>;
    using Source = std::remove_cvref_t<From>;
    if constexpr (std::is_const_v<std::remove_reference_t<To>>) return false;
    else if constexpr (std::is_same_v<Target, Source>) return std::is_assignable_v<To&, From>;
    else if constexpr (commandStringDescriptor<Target>) {
        return (commandStringDescriptor<Source> || std::same_as<Source, std::nullptr_t>) &&
            (std::same_as<Target, nicxlive::core::common::NullableString> ||
                std::is_constructible_v<Target, compat::RetainedString>);
    }
    else if constexpr (std::is_same_v<Target, std::string>) return std::is_same_v<Source, std::nullptr_t>;
    else if constexpr (CommandSharedPointer<Target>::value) {
        if constexpr (std::is_same_v<Source, std::nullptr_t>) return true;
        else if constexpr (CommandSharedPointer<Source>::value) return std::is_assignable_v<To&, From>;
        else return false;
    } else if constexpr (std::is_arithmetic_v<Target>) {
        using Numeric = typename CommandNumericSource<Source>::Type;
        if constexpr (!std::is_arithmetic_v<Numeric>) return false;
        else if constexpr (std::is_same_v<Target, bool>) return std::is_same_v<Numeric, bool>;
        else if constexpr (std::is_floating_point_v<Target>) return true;
        else return std::is_integral_v<Numeric> && sizeof(Target) >= sizeof(Numeric);
    } else return false;
}

template<class T> constexpr auto commandNumericValue(T value) {
    if constexpr (std::is_enum_v<T>) return commandNumericValue(static_cast<std::underlying_type_t<T>>(value));
    else if constexpr (std::same_as<T, char> || std::same_as<T, char8_t> ||
            std::same_as<T, char16_t> || std::same_as<T, char32_t> || std::same_as<T, wchar_t>)
        return static_cast<std::make_unsigned_t<T>>(value);
    else return value;
}

template<class To, class From> requires(acceptsBuiltinCommandValue<To, From&&>())
void assignBuiltinCommandValue(To& target, From&& value) {
    using Target = std::remove_cvref_t<To>;
    using Source = std::remove_cvref_t<From>;
    if constexpr (commandStringDescriptor<Target> && !std::same_as<Target, Source>) {
        if constexpr (std::same_as<Target, nicxlive::core::common::NullableString>) {
            if constexpr (std::derived_from<Source, compat::RetainedStringSlice>)
                target = value.retainedString().toNullableString();
            else target = compat::RetainedString(value).toNullableString();
        }
        else if constexpr (std::derived_from<Source, compat::RetainedStringSlice>)
            target = Target(value.retainedString());
        else target = Target(compat::RetainedString(value));
    }
    else if constexpr (std::is_same_v<Target, std::string> && std::is_same_v<Source, std::nullptr_t>) target.clear();
    else if constexpr (std::is_arithmetic_v<Target> && (std::is_arithmetic_v<Source> || std::is_enum_v<Source>)) {
        target = static_cast<Target>(commandNumericValue(value));
    } else target = std::forward<From>(value);
}

template<class To, class From> requires(acceptsBuiltinCommandInitialization<To, From&&>())
void initializeBuiltinCommandValue(To& target, From&& value) {
    using Target = std::remove_cvref_t<To>;
    using Source = std::remove_cvref_t<From>;
    // These defining carriers have passive Source same-type copy construction.
    // Their native transport copy preserves the existing C++ subobject lifetime,
    // including during exceptions. Custom Source construction uses its own
    // SourceCommandInitialization body, never SourceCommandAssignment.
    if constexpr (std::same_as<Source, std::nullptr_t> &&
            CommandInitializationDescriptor<Target>::value) {
        // These exact dynamic-array, AA and delegate carriers default to the
        // real null descriptor without allocating or invoking user callbacks.
        target = Target{};
    } else if constexpr (commandStringDescriptor<Target> && !std::same_as<Target, Source>) {
        if constexpr (std::same_as<Target, nicxlive::core::common::NullableString>) {
            if constexpr (std::derived_from<Source, compat::RetainedStringSlice>)
                target = value.retainedString().toNullableString();
            else target = compat::RetainedString(value).toNullableString();
        } else if constexpr (std::derived_from<Source, compat::RetainedStringSlice>)
            target = Target(value.retainedString());
        else target = Target(compat::RetainedString(value));
    } else if constexpr (std::is_arithmetic_v<Target> &&
            (std::is_arithmetic_v<Source> || std::is_enum_v<Source>)) {
        target = static_cast<Target>(commandNumericValue(value));
    } else target = std::forward<From>(value);
}

template<class To, class From> void assignCommandValue(To& target, From&& value) {
    SourceCommandAssignment<To, From&&>::assign(target, std::forward<From>(value));
}

template<class To, class From> constexpr bool acceptsCommandValue() {
    using Target = std::remove_cvref_t<To>;
    using Source = std::remove_cvref_t<From>;
    // Source type identity is independent of the later assignment expression.
    if constexpr (std::same_as<std::remove_reference_t<To>, std::remove_reference_t<From>>) return true;
    else if constexpr (requires { sizeof(SourceCommandImplicitConversion<To, From>); }) {
        if constexpr (requires { SourceCommandImplicitConversion<To, From>::convert(std::declval<From>()); }) {
            using Actual = decltype(SourceCommandImplicitConversion<To, From>::convert(std::declval<From>()));
            if constexpr (std::same_as<std::remove_reference_t<Actual>, std::remove_reference_t<To>>) return true;
            else return acceptsCommandValue<To, Actual>();
        } else return false;
    } else if constexpr (commandBuiltinValue<Target> && commandBuiltinValue<Source>) {
        return acceptsBuiltinCommandValue<To, From>();
    } else {
        static_assert(!std::is_same_v<To, To>,
            "Missing defining Source implicit-conversion mapping for command arguments");
    }
}

template<class Storage, class Args, std::size_t... I>
constexpr bool assignableCommandFields(std::index_sequence<I...>) {
    using Descriptors = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    return (acceptsCommandValue<typename std::tuple_element_t<I, Descriptors>::Type,
        std::tuple_element_t<I, Args>>() && ...);
}
template<class Storage, class... Args> constexpr bool acceptsCommandFields() {
    constexpr auto count = std::tuple_size_v<decltype(CommandFields<Storage>::descriptors)>;
    if constexpr (!validCommandFields<Storage>() || count != sizeof...(Args)) return false;
    else return assignableCommandFields<Storage, std::tuple<Args...>>(std::make_index_sequence<count>{});
}
}

template<class To, class From>
struct SourceCommandAssignment<To, From,
        std::enable_if_t<detail::acceptsBuiltinCommandValue<To, From>() ||
            (detail::commandBuiltinValue<std::remove_cvref_t<To>> &&
                detail::commandBuiltinValue<std::remove_cvref_t<From>>)>> {
    // A translated builtin rejection has a defining mapping but no viable
    // operation. It is distinct from an absent user-defined Source mapping.
    static void assign(To& target, From value)
        requires (detail::acceptsBuiltinCommandValue<To, From>()) {
        detail::assignBuiltinCommandValue(target, std::forward<From>(value));
    }
};

template<class To, class From> consteval bool ngCommandAssignmentSupported() {
    if constexpr (std::is_void_v<To> || std::is_void_v<From>) return false;
    else {
        using Assignment = SourceCommandAssignment<To, From>;
        static_assert(requires { sizeof(Assignment); },
            "Missing defining Source assignment mapping for command arguments");
        return requires(To& target) { Assignment::assign(target, std::declval<From>()); };
    }
}

template<class To, class From>
struct SourceCommandInitialization<To, From, std::enable_if_t<
        !std::is_const_v<To> && !std::is_volatile_v<To> &&
        (!std::same_as<std::remove_cvref_t<To>, std::remove_cvref_t<From>> ||
            std::is_assignable_v<To&, From>) &&
        detail::CommandInitializationValue<std::remove_cvref_t<To>>::value &&
        detail::CommandInitializationValue<std::remove_cvref_t<From>>::value &&
        (std::same_as<std::remove_cvref_t<To>, std::remove_cvref_t<From>> ||
            (detail::commandBuiltinValue<std::remove_cvref_t<To>> &&
                detail::commandBuiltinValue<std::remove_cvref_t<From>>) ||
            ((detail::CommandInitializationDescriptor<std::remove_cvref_t<To>>::value ||
                std::is_pointer_v<std::remove_cvref_t<To>>) &&
                std::same_as<std::remove_cvref_t<From>, std::nullptr_t>))>> {
    static void construct(To& target, From value)
        requires(detail::acceptsBuiltinCommandInitialization<To, From>()) {
        detail::initializeBuiltinCommandValue(target, std::forward<From>(value));
    }
};

template<class To, class From> consteval bool ngCommandInitializationSupported() {
    if constexpr (std::is_void_v<To> || std::is_void_v<From>) return false;
    else {
        using Initialization = SourceCommandInitialization<To, From>;
        static_assert(requires { sizeof(Initialization); },
            "Missing defining Source constructor-field initialization mapping");
        return requires(To& target) { Initialization::construct(target, std::declval<From>()); };
    }
}

// Only genuine passive value/descriptor carriers receive this copy body.
// A custom postblit supplies its real defining Source parameter-copy program.
template<class To, class From>
struct SourceCommandParameterCopy<To, From, std::enable_if_t<
        std::same_as<To, std::decay_t<From>> && detail::CommandInitializationValue<To>::value>> {
    static auto copy(From value) requires(std::is_constructible_v<To, From>) {
        return To(std::forward<From>(value));
    }
};

template<class To, class From> consteval bool ngCommandParameterCopySupported() {
    using Copy = SourceCommandParameterCopy<To, From>;
    static_assert(requires { sizeof(Copy); },
        "Missing defining Source constructor parameter-copy mapping");
    return requires { { Copy::copy(std::declval<From>()) } -> std::same_as<To>; };
}

namespace detail {
template<class Storage, class Arguments, std::size_t... I>
consteval bool commandFieldAssignmentsSupported(std::index_sequence<I...>) {
    using Descriptors = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    return (ngCommandAssignmentSupported<
        std::remove_reference_t<decltype(std::tuple_element_t<I, Descriptors>::get(std::declval<Storage&>()))>,
        std::tuple_element_t<I, Arguments>>() && ...);
}

template<class Storage, class Arguments, std::size_t... I>
consteval bool commandFieldInitializationsSupported(std::index_sequence<I...>) {
    using Descriptors = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    return (ngCommandInitializationSupported<
        std::remove_reference_t<decltype(std::tuple_element_t<I, Descriptors>::get(std::declval<Storage&>()))>,
        std::tuple_element_t<I, Arguments>>() && ...);
}
}

// Checks the same field selection and assignment operations used below. The
// caller supplies their actual expression types, including reference qualifiers.
// Runtime auto-ref arguments are named lvalues. Constructor initialization has
// its independent operation program below.
template<class Storage, class... Args> consteval bool ngCommandFieldsAssignmentSupported() {
    constexpr auto count = std::tuple_size_v<decltype(CommandFields<Storage>::descriptors)>;
    if constexpr (!detail::validCommandFields<Storage>() || count != sizeof...(Args)) return false;
    else return detail::commandFieldAssignmentsSupported<Storage, std::tuple<Args...>>(
        std::make_index_sequence<count>{});
}

template<class Storage, class... Args>
    requires (ngCommandFieldsAssignmentSupported<Storage, Args&&...>())
void ngAssignCommandFields(Storage& storage, Args&&... args) {
    using Descriptors = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    static_assert(detail::validCommandFields<Storage>(), "Invalid command argument fields");
    static_assert(sizeof...(Args) == std::tuple_size_v<Descriptors>, "Command argument count mismatch");
    if constexpr (sizeof...(Args) == std::tuple_size_v<Descriptors>) {
        auto values = std::forward_as_tuple(std::forward<Args>(args)...);
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            ([&] {
                auto&& target = std::tuple_element_t<I, Descriptors>::get(storage);
                detail::assignCommandValue(target, std::get<I>(std::move(values)));
            }(), ...);
        }(std::index_sequence_for<Args...>{});
    }
}

template<class Storage, class... Args> consteval bool ngCommandFieldsInitializationSupported() {
    constexpr auto count = std::tuple_size_v<decltype(CommandFields<Storage>::descriptors)>;
    if constexpr (!detail::validCommandFields<Storage>() || count != sizeof...(Args)) return false;
    else return detail::commandFieldInitializationsSupported<Storage, std::tuple<Args...>>(
        std::make_index_sequence<count>{});
}

// Only the constructor invokes this operation on its still-initial .init fields.
template<class Storage, class... Args>
    requires(ngCommandFieldsInitializationSupported<Storage, Args&&...>())
void ngInitializeCommandFields(Storage& storage, Args&&... args) {
    using Descriptors = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    auto values = std::forward_as_tuple(std::forward<Args>(args)...);
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        ([&] {
            auto&& target = std::tuple_element_t<I, Descriptors>::get(storage);
            using Target = std::remove_reference_t<decltype(target)>;
            auto&& value = std::get<I>(std::move(values));
            SourceCommandInitialization<Target, decltype(value)>::construct(
                target, std::forward<decltype(value)>(value));
        }(), ...);
    }(std::index_sequence_for<Args...>{});
}

} // namespace nijigenerate::commands
