#pragma once

#include <compat/object_metadata.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

namespace nijigenerate::commands {

// Counted Source string bytes, including an independent terminal NUL for native
// consumers. This is a value producer, not a C++20 static_assert text emitter.
template<std::size_t Size> struct SourceDiagnosticText {
    char bytes[Size + 1]{};
    consteval SourceDiagnosticText() = default;
    template<std::size_t N> requires (N == Size + 1)
    consteval SourceDiagnosticText(const char (&text)[N]) {
        for (std::size_t index = 0; index < Size; ++index) bytes[index] = text[index];
    }
    static constexpr std::size_t size() noexcept { return Size; }
    constexpr std::string_view view() const noexcept { return {bytes, Size}; }
};
template<std::size_t N> SourceDiagnosticText(const char (&)[N]) -> SourceDiagnosticText<N - 1>;

template<std::size_t Left, std::size_t Right>
consteval auto operator+(SourceDiagnosticText<Left> left, SourceDiagnosticText<Right> right) {
    SourceDiagnosticText<Left + Right> result;
    for (std::size_t index = 0; index < Left; ++index) result.bytes[index] = left.bytes[index];
    for (std::size_t index = 0; index < Right; ++index) result.bytes[Left + index] = right.bytes[index];
    return result;
}

template<class C> concept HasLiteralCommandTypeStringOf = requires {
    nicxlive::core::d_runtime::SourceTypeSpellings<C>::unqualifiedElement;
    requires std::is_array_v<decltype(nicxlive::core::d_runtime::SourceTypeSpellings<C>::unqualifiedElement)>;
    requires std::same_as<std::remove_cv_t<std::remove_extent_t<
        decltype(nicxlive::core::d_runtime::SourceTypeSpellings<C>::unqualifiedElement)>>, char>;
    std::integral_constant<char,
        nicxlive::core::d_runtime::SourceTypeSpellings<C>::unqualifiedElement[0]>{};
};
template<class C> concept HasLiteralCommandClassStringOf =
    std::is_class_v<C> && HasLiteralCommandTypeStringOf<C>;

// Type.stringof uses the selected unqualified type printer. Consume its actual
// defining literal record; runtime-only metadata and absent records stay absent.
template<HasLiteralCommandTypeStringOf C> consteval auto ngCommandTypeStringOf() {
    return SourceDiagnosticText{nicxlive::core::d_runtime::SourceTypeSpellings<C>::unqualifiedElement};
}
template<HasLiteralCommandClassStringOf C> consteval auto ngCommandClassStringOf() {
    return ngCommandTypeStringOf<C>();
}
template<HasLiteralCommandTypeStringOf K> consteval auto ngInitializerDispatchFailureText() {
    return SourceDiagnosticText{"No command initializer dispatch for key type: "} + ngCommandTypeStringOf<K>();
}
template<HasLiteralCommandClassStringOf C> consteval auto ngBaseExArgsFailureText() {
    return ngCommandClassStringOf<C>() + SourceDiagnosticText{" is not derived from ExCommand!T"};
}

// Tuple lengths and untyped static-foreach tuple indices are Source size_t.
// Its printer is selected by the Source target, never by the native carrier.
template<std::uint64_t Value, unsigned SourceSizeBits> consteval auto ngCommandSizeStringOf() {
    static_assert(SourceSizeBits == 32 || SourceSizeBits == 64,
        "A defining Source size_t width is required for command diagnostics");
    static_assert(SourceSizeBits != 32 || Value <= std::numeric_limits<std::uint32_t>::max(),
        "Command diagnostic size does not fit the defining Source size_t");
    constexpr std::size_t digits = [] {
        std::size_t count = 1;
        auto remaining = Value;
        while (remaining >= 10) { remaining /= 10; ++count; }
        return count;
    }();
    SourceDiagnosticText<digits + (SourceSizeBits == 32 ? 1 : 2)> result;
    auto remaining = Value;
    for (std::size_t offset = digits; offset != 0; --offset) {
        result.bytes[offset - 1] = static_cast<char>('0' + remaining % 10);
        remaining /= 10;
    }
    if constexpr (SourceSizeBits == 32) result.bytes[digits] = 'u';
    else { result.bytes[digits] = 'L'; result.bytes[digits + 1] = 'U'; }
    return result;
}

template<HasLiteralCommandClassStringOf C, std::uint64_t Expected, std::uint64_t Actual,
    unsigned SourceSizeBits> consteval auto ngCommandArgumentCountFailureText() {
    return ngCommandClassStringOf<C>() + SourceDiagnosticText{": expected "}
        + ngCommandSizeStringOf<Expected, SourceSizeBits>() + SourceDiagnosticText{" args, got "}
        + ngCommandSizeStringOf<Actual, SourceSizeBits>();
}

template<HasLiteralCommandClassStringOf C, std::uint64_t Index, unsigned SourceSizeBits,
    HasLiteralCommandTypeStringOf Actual, HasLiteralCommandTypeStringOf Expected>
consteval auto ngCommandArgumentTypeFailureText() {
    return ngCommandClassStringOf<C>() + SourceDiagnosticText{": argument #"}
        + ngCommandSizeStringOf<Index, SourceSizeBits>() + SourceDiagnosticText{" type mismatch: got "}
        + ngCommandTypeStringOf<Actual>() + SourceDiagnosticText{", expected "}
        + ngCommandTypeStringOf<Expected>();
}

template<unsigned SourceSizeBits, SourceDiagnosticText ClassSpelling,
    SourceDiagnosticText Message, auto Emitter> struct SourceCommandCountOrigin {
    inline static constexpr unsigned sourceSizeBits = SourceSizeBits;
    inline static constexpr auto sourceClassStringOf = ClassSpelling;
    inline static constexpr auto sourceMessage = Message;
    template<bool Accepted> static consteval bool requireCount() {
        return Emitter.template operator()<Accepted>();
    }
};

namespace detail {
template<class T> struct SourceCommandCountOriginTraits {
    inline static constexpr bool supplied = false;
};
template<unsigned Bits, SourceDiagnosticText Class, SourceDiagnosticText Message, auto Emitter>
struct SourceCommandCountOriginTraits<SourceCommandCountOrigin<Bits, Class, Message, Emitter>> {
    inline static constexpr bool supplied = true;
};
}

template<std::size_t Index, unsigned Bits, SourceDiagnosticText Class,
    SourceDiagnosticText Actual, SourceDiagnosticText Expected, SourceDiagnosticText Message,
    auto Emitter> struct SourceCommandArgumentOrigin {
    inline static constexpr auto index = Index;
    inline static constexpr auto sourceSizeBits = Bits;
    inline static constexpr auto sourceClassStringOf = Class;
    inline static constexpr auto actualStringOf = Actual;
    inline static constexpr auto expectedStringOf = Expected;
    inline static constexpr auto sourceMessage = Message;
    template<bool Accepted> static consteval bool requireType() {
        return Emitter.template operator()<Accepted>();
    }
};

namespace detail {
template<class T> struct SourceCommandArgumentOriginTraits {
    inline static constexpr bool supplied = false;
};
template<std::size_t Index, unsigned Bits, SourceDiagnosticText Class, SourceDiagnosticText Actual,
    SourceDiagnosticText Expected, SourceDiagnosticText Message, auto Emitter>
struct SourceCommandArgumentOriginTraits<SourceCommandArgumentOrigin<Index, Bits, Class,
    Actual, Expected, Message, Emitter>> {
    inline static constexpr bool supplied = true;
};
}

template<class C, std::size_t Index, class Actual, class Expected, auto SourceOrigin>
consteval bool ngRequireSourceCommandArgumentType() {
    using Origin = std::remove_cv_t<decltype(SourceOrigin)>;
    if constexpr (detail::SourceCommandArgumentOriginTraits<Origin>::supplied) {
        constexpr bool validSize = Origin::sourceSizeBits == 64 || (Origin::sourceSizeBits == 32 &&
            Index <= std::numeric_limits<std::uint32_t>::max());
        if constexpr (!validSize) {
            static_assert(validSize, "Source argument-type origin has no valid defining size_t representation");
            return true;
        } else {
            constexpr bool namesMatch = [] {
                if constexpr (HasLiteralCommandClassStringOf<C>)
                    if (Origin::sourceClassStringOf.view() != ngCommandClassStringOf<C>().view()) return false;
                if constexpr (HasLiteralCommandTypeStringOf<Actual>)
                    if (Origin::actualStringOf.view() != ngCommandTypeStringOf<Actual>().view()) return false;
                if constexpr (HasLiteralCommandTypeStringOf<Expected>)
                    if (Origin::expectedStringOf.view() != ngCommandTypeStringOf<Expected>().view()) return false;
                return true;
            }();
            constexpr auto message = Origin::sourceClassStringOf + SourceDiagnosticText{": argument #"}
                + ngCommandSizeStringOf<Index, Origin::sourceSizeBits>()
                + SourceDiagnosticText{" type mismatch: got "} + Origin::actualStringOf
                + SourceDiagnosticText{", expected "} + Origin::expectedStringOf;
            constexpr bool originMatches = namesMatch && Origin::index == Index
                && message.view() == Origin::sourceMessage.view();
            if constexpr (!originMatches) {
                static_assert(originMatches, "Source argument-type origin disagrees with the rejected argument");
                return true;
            } else return Origin::template requireType<false>();
        }
    } else return false;
}

template<class C, std::uint64_t Expected, std::uint64_t Actual, auto SourceOrigin = nullptr>
consteval bool ngRequireSourceCommandArgumentCount() {
    using Origin = std::remove_cv_t<decltype(SourceOrigin)>;
    if constexpr (detail::SourceCommandCountOriginTraits<Origin>::supplied) {
        constexpr bool validSize = Origin::sourceSizeBits == 64 || (Origin::sourceSizeBits == 32 &&
            Expected <= std::numeric_limits<std::uint32_t>::max() &&
            Actual <= std::numeric_limits<std::uint32_t>::max());
        if constexpr (!validSize) {
            static_assert(validSize, "Source argument-count origin has no valid defining size_t representation");
            return true;
        } else {
            constexpr bool classMatches = [] {
                if constexpr (HasLiteralCommandClassStringOf<C>)
                    return Origin::sourceClassStringOf.view() == ngCommandClassStringOf<C>().view();
                else return true;
            }();
            constexpr auto message = Origin::sourceClassStringOf + SourceDiagnosticText{": expected "}
                + ngCommandSizeStringOf<Expected, Origin::sourceSizeBits>()
                + SourceDiagnosticText{" args, got "}
                + ngCommandSizeStringOf<Actual, Origin::sourceSizeBits>();
            constexpr bool countsMatch = message.view() == Origin::sourceMessage.view();
            if constexpr (!classMatches) {
                static_assert(classMatches, "Source argument-count origin disagrees with the defining class spelling");
                return true;
            } else if constexpr (!countsMatch) {
                static_assert(countsMatch, "Source argument-count origin disagrees with the actual argument counts");
                return true;
            } else return Origin::template requireCount<false>();
        }
    } else return false;
}

// Expression is the actual selected alias/expression .stringof result supplied
// at its defining boundary. An auto value NTTP cannot recover that identity.
template<SourceDiagnosticText Expression> consteval auto ngCommandByIdFailureText() {
    return SourceDiagnosticText{"No registered command type for id: "} + Expression
        + SourceDiagnosticText{". Add its class to RegisteredCommands."};
}

template<SourceDiagnosticText Expression, auto BaseEmitter, auto DispatchEmitter, auto MapEmitter = nullptr>
struct SourceCommandTypeOrigin {
    inline static constexpr auto sourceStringOf = Expression;
    template<bool Accepted> static consteval bool requireExCommandBase() {
        return BaseEmitter.template operator()<Accepted>();
    }
    template<bool Accepted> static consteval bool requireInitializerDispatch() {
        return DispatchEmitter.template operator()<Accepted>();
    }
    template<bool Accepted> static consteval bool requireCommandsMap() {
        if constexpr (std::same_as<std::remove_cv_t<decltype(MapEmitter)>, std::nullptr_t>) return false;
        else return MapEmitter.template operator()<Accepted>();
    }
};

template<SourceDiagnosticText Expression, auto Emitter> struct SourceAssociativeArrayOrigin {
    inline static constexpr auto sourceStringOf = Expression;
    template<bool Accepted> static consteval bool requireAssociativeArray() {
        return Emitter.template operator()<Accepted>();
    }
};

// A defining expression supplies its actual typed value and a literal emitter.
// The empty structural carrier retains that identity through auto-NTTP recursion.
template<auto Value, SourceDiagnosticText Expression, auto Emitter,
    class SourceClass = void, class SourceKey = std::remove_cv_t<decltype(Value)>, auto NamingEmitter = nullptr>
struct SourceCommandIdExpression {
    using CommandType = SourceClass;
    using KeyType = SourceKey;
    inline static constexpr auto value = Value;
    inline static constexpr auto sourceStringOf = Expression;
    template<bool Accepted> static consteval bool requireRegisteredCommand() {
        return Emitter.template operator()<Accepted>();
    }
    template<bool Accepted> static consteval bool requireClassName() {
        if constexpr (std::same_as<std::remove_cv_t<decltype(NamingEmitter)>, std::nullptr_t>) return false;
        else return NamingEmitter.template operator()<Accepted>();
    }
};

// Read actual alias storage separately; it need not be a structural constant.
template<SourceDiagnosticText Expression, class SourceKey, class SourceClass, auto NamingEmitter,
    auto RegisteredEmitter = nullptr>
struct SourceCommandAliasExpression {
    using CommandType = SourceClass;
    using KeyType = SourceKey;
    inline static constexpr auto sourceStringOf = Expression;
    template<bool Accepted> static consteval bool requireClassName() {
        return NamingEmitter.template operator()<Accepted>();
    }
    template<bool Accepted> static consteval bool requireRegisteredCommand() {
        if constexpr (std::same_as<std::remove_cv_t<decltype(RegisteredEmitter)>, std::nullptr_t>) return false;
        else return RegisteredEmitter.template operator()<Accepted>();
    }
};
namespace detail {
template<class T> struct SourceCommandTypeOriginTraits {
    inline static constexpr bool supplied = false;
};
template<SourceDiagnosticText Expression, auto BaseEmitter, auto DispatchEmitter, auto MapEmitter>
struct SourceCommandTypeOriginTraits<SourceCommandTypeOrigin<Expression, BaseEmitter, DispatchEmitter, MapEmitter>> {
    inline static constexpr bool supplied = true;
};

template<class T> struct SourceAssociativeArrayOriginTraits {
    inline static constexpr bool supplied = false;
};
template<SourceDiagnosticText Expression, auto Emitter>
struct SourceAssociativeArrayOriginTraits<SourceAssociativeArrayOrigin<Expression, Emitter>> {
    inline static constexpr bool supplied = true;
};
template<class T> struct SourceCommandIdExpressionTraits {
    inline static constexpr bool supplied = false;
};
template<auto Value, SourceDiagnosticText Expression, auto Emitter, class SourceClass, class SourceKey,
    auto NamingEmitter>
struct SourceCommandIdExpressionTraits<SourceCommandIdExpression<Value, Expression, Emitter, SourceClass,
    SourceKey, NamingEmitter>> {
    inline static constexpr bool supplied = true;
};

template<class T> struct SourceCommandAliasExpressionTraits {
    inline static constexpr bool supplied = false;
};
template<SourceDiagnosticText Expression, class SourceKey, class SourceClass, auto NamingEmitter,
    auto RegisteredEmitter>
struct SourceCommandAliasExpressionTraits<SourceCommandAliasExpression<Expression, SourceKey, SourceClass,
    NamingEmitter, RegisteredEmitter>> {
    inline static constexpr bool supplied = true;
};

template<class T> inline constexpr bool sourceCommandCallOrigin =
    SourceCommandIdExpressionTraits<T>::supplied || SourceCommandAliasExpressionTraits<T>::supplied;
}
template<auto Id>
    requires (!detail::SourceCommandAliasExpressionTraits<std::remove_cv_t<decltype(Id)>>::supplied)
constexpr decltype(auto) ngSourceCommandIdValue() {
    if constexpr (detail::SourceCommandIdExpressionTraits<std::remove_cv_t<decltype(Id)>>::supplied)
        return (std::remove_cv_t<decltype(Id)>::value);
    else return Id;
}

template<auto Id> consteval auto ngSourceCommandIdKeyType() {
    using Origin = std::remove_cv_t<decltype(Id)>;
    if constexpr (detail::sourceCommandCallOrigin<Origin>)
        return std::type_identity<typename Origin::KeyType>{};
    else return std::type_identity<std::remove_cv_t<decltype(Id)>>{};
}
template<auto Id>
    requires detail::sourceCommandCallOrigin<std::remove_cv_t<decltype(Id)>>
consteval auto ngSourceCommandIdStringOf() {
    return std::remove_cv_t<decltype(Id)>::sourceStringOf;
}
template<auto Id>
    requires detail::sourceCommandCallOrigin<std::remove_cv_t<decltype(Id)>>
consteval auto ngRegisteredSourceCommandFailureText() {
    return ngCommandByIdFailureText<ngSourceCommandIdStringOf<Id>()>();
}
template<auto Id> consteval bool ngRequireRegisteredSourceCommand() {
    using Origin = std::remove_cv_t<decltype(Id)>;
    if constexpr (detail::sourceCommandCallOrigin<Origin>)
        return Origin::template requireRegisteredCommand<false>();
    else return false;
}

template<auto Id> consteval bool ngRequireSourceCommandClassName() {
    using Origin = std::remove_cv_t<decltype(Id)>;
    if constexpr (detail::sourceCommandCallOrigin<Origin>)
        return Origin::template requireClassName<false>();
    else return false;
}
template<class C, auto SourceOrigin = nullptr> consteval bool ngRequireSourceExCommandBase() {
    using Supplied = std::remove_cv_t<decltype(SourceOrigin)>;
    using Declared = nicxlive::core::d_runtime::SourceTypeSpellings<C>;
    if constexpr (detail::SourceCommandTypeOriginTraits<Supplied>::supplied) {
        constexpr bool matches = [] {
            if constexpr (HasLiteralCommandTypeStringOf<C>)
                return Supplied::sourceStringOf.view() == ngCommandTypeStringOf<C>().view();
            else return true;
        }();
        if constexpr (!matches) {
            static_assert(matches, "Source diagnostic origin disagrees with the defining type spelling");
            return true;
        } else return Supplied::template requireExCommandBase<false>();
    } else if constexpr (std::same_as<Supplied, std::nullptr_t> &&
            requires { Declared::template requireExCommandBase<false>(); })
        return Declared::template requireExCommandBase<false>();
    else return false;
}
template<class K, auto SourceOrigin = nullptr> consteval bool ngRequireSourceInitializerDispatch() {
    using Supplied = std::remove_cv_t<decltype(SourceOrigin)>;
    using Declared = nicxlive::core::d_runtime::SourceTypeSpellings<K>;
    if constexpr (detail::SourceCommandTypeOriginTraits<Supplied>::supplied) {
        constexpr bool matches = [] {
            if constexpr (HasLiteralCommandTypeStringOf<K>)
                return Supplied::sourceStringOf.view() == ngCommandTypeStringOf<K>().view();
            else return true;
        }();
        if constexpr (!matches) {
            static_assert(matches, "Source diagnostic origin disagrees with the defining key spelling");
            return true;
        } else return Supplied::template requireInitializerDispatch<false>();
    } else if constexpr (std::same_as<Supplied, std::nullptr_t> &&
            requires { Declared::template requireInitializerDispatch<false>(); })
        return Declared::template requireInitializerDispatch<false>();
    else return false;
}

template<class K, auto SourceOrigin = nullptr> consteval bool ngRequireSourceCommandsMap() {
    using Supplied = std::remove_cv_t<decltype(SourceOrigin)>;
    using Declared = nicxlive::core::d_runtime::SourceTypeSpellings<K>;
    if constexpr (detail::SourceCommandTypeOriginTraits<Supplied>::supplied) {
        constexpr bool matches = [] {
            if constexpr (HasLiteralCommandTypeStringOf<K>)
                return Supplied::sourceStringOf.view() == ngCommandTypeStringOf<K>().view();
            else return true;
        }();
        if constexpr (!matches) {
            static_assert(matches, "Source diagnostic origin disagrees with the defining map key spelling");
            return true;
        } else return Supplied::template requireCommandsMap<false>();
    } else if constexpr (std::same_as<Supplied, std::nullptr_t> &&
            requires { Declared::template requireCommandsMap<false>(); })
        return Declared::template requireCommandsMap<false>();
    else return false;
}

template<auto SourceOrigin> consteval bool ngRequireSourceAssociativeArray() {
    using Origin = std::remove_cv_t<decltype(SourceOrigin)>;
    if constexpr (detail::SourceAssociativeArrayOriginTraits<Origin>::supplied)
        return Origin::template requireAssociativeArray<false>();
    else return false;
}

}

// SourceSpelling must be the defining Source semantic printer's literal. Never
// stringify NativeValue: its C++ tokens are not the Source alias expression.
#define NG_SOURCE_COMMAND_ID(NativeValue, SourceSpelling) \
    ::nijigenerate::commands::SourceCommandIdExpression<(NativeValue), \
        ::nijigenerate::commands::SourceDiagnosticText{SourceSpelling}, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No registered command type for id: " SourceSpelling \
                ". Add its class to RegisteredCommands."); \
            return true; \
        }, void, std::remove_cvref_t<decltype(NativeValue)>, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No command class found by naming convention: " \
                SourceSpelling "Command (derived from id: " SourceSpelling ")"); \
            return true; \
        }>{}

// The defining symbol lookup can supply its actual resolved class and key type.
// Neither can in general be reconstructed from an enum value or its first alias.
#define NG_SOURCE_TYPED_COMMAND_ID(NativeValue, SourceSpelling, SourceKey, ...) \
    ::nijigenerate::commands::SourceCommandIdExpression<(NativeValue), \
        ::nijigenerate::commands::SourceDiagnosticText{SourceSpelling}, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No registered command type for id: " SourceSpelling \
                ". Add its class to RegisteredCommands."); \
            return true; \
        }, __VA_ARGS__, SourceKey, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No command class found by naming convention: " \
                SourceSpelling "Command (derived from id: " SourceSpelling ")"); \
            return true; \
        }>{}

#define NG_SOURCE_COMMAND_ALIAS(SourceSpelling, SourceKey, ...) \
    ::nijigenerate::commands::SourceCommandAliasExpression< \
        ::nijigenerate::commands::SourceDiagnosticText{SourceSpelling}, SourceKey, __VA_ARGS__, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No command class found by naming convention: " \
                SourceSpelling "Command (derived from id: " SourceSpelling ")"); \
            return true; \
        }, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No registered command type for id: " SourceSpelling \
                ". Add its class to RegisteredCommands."); \
            return true; \
        }>{}

#define NG_SOURCE_ASSOCIATIVE_ARRAY_ORIGIN(SourceSpelling) \
    ::nijigenerate::commands::SourceAssociativeArrayOrigin< \
        ::nijigenerate::commands::SourceDiagnosticText{SourceSpelling}, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, SourceSpelling " is not an associative array"); \
            return true; \
        }>{}

// Length spellings are the defining Source printer results, including u or LU.
#define NG_SOURCE_COMMAND_COUNT_ORIGIN(SourceBits, ClassSpelling, ExpectedSpelling, ActualSpelling) \
    ::nijigenerate::commands::SourceCommandCountOrigin<(SourceBits), \
        ::nijigenerate::commands::SourceDiagnosticText{ClassSpelling}, \
        ::nijigenerate::commands::SourceDiagnosticText{ClassSpelling ": expected " \
            ExpectedSpelling " args, got " ActualSpelling}, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, ClassSpelling ": expected " ExpectedSpelling \
                " args, got " ActualSpelling); \
            return true; \
        }>{}

#define NG_SOURCE_COMMAND_ARGUMENT_ORIGIN(Index, SourceBits, ClassSpelling, IndexSpelling, \
        ActualSpelling, ExpectedSpelling) \
    ::nijigenerate::commands::SourceCommandArgumentOrigin<(Index), (SourceBits), \
        ::nijigenerate::commands::SourceDiagnosticText{ClassSpelling}, \
        ::nijigenerate::commands::SourceDiagnosticText{ActualSpelling}, \
        ::nijigenerate::commands::SourceDiagnosticText{ExpectedSpelling}, \
        ::nijigenerate::commands::SourceDiagnosticText{ClassSpelling ": argument #" IndexSpelling \
            " type mismatch: got " ActualSpelling ", expected " ExpectedSpelling}, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, ClassSpelling ": argument #" IndexSpelling \
                " type mismatch: got " ActualSpelling ", expected " ExpectedSpelling); \
            return true; \
        }>{}

// Used inside the actual defining SourceTypeSpellings specialization. Its
// existing unqualified-name literal supplies both values and authored errors.
#define NG_SOURCE_TYPE_DIAGNOSTIC_MEMBERS(SourceSpelling) \
    template<bool Accepted> static consteval bool requireExCommandBase() { \
        static_assert(Accepted, SourceSpelling " is not derived from ExCommand!T"); \
        return true; \
    } \
    template<bool Accepted> static consteval bool requireInitializerDispatch() { \
        static_assert(Accepted, "No command initializer dispatch for key type: " SourceSpelling); \
        return true; \
    } \
    template<bool Accepted> static consteval bool requireCommandsMap() { \
        static_assert(Accepted, "No commands map found for key type: " SourceSpelling); \
        return true; \
    }

#define NG_SOURCE_COMMAND_TYPE_ORIGIN(SourceSpelling) \
    ::nijigenerate::commands::SourceCommandTypeOrigin< \
        ::nijigenerate::commands::SourceDiagnosticText{SourceSpelling}, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, SourceSpelling " is not derived from ExCommand!T"); \
            return true; \
        }, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No command initializer dispatch for key type: " SourceSpelling); \
            return true; \
        }, \
        []<bool Accepted>() consteval -> bool { \
            static_assert(Accepted, "No commands map found for key type: " SourceSpelling); \
            return true; \
        }>{}
