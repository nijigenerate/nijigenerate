#pragma once

#include <compat/object_metadata.hpp>
#include "source_reference.hpp"
#include "source_call_arguments.hpp"
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {
template<class Signature> class SharedDelegate;
template<class Callable> struct SourceCallPreparation;
namespace detail {

template<class T> struct SourceCallableReference : std::false_type {};

template<class T> struct SourceCallableReference<std::reference_wrapper<T>> : std::true_type {};

template<class Function> constexpr decltype(auto) sourceCallableTarget(Function&& function) noexcept {
    if constexpr (SourceCallableReference<std::remove_cvref_t<Function>>::value) return function.get();
    else return std::forward<Function>(function);
}

template<class Function, class... Args> concept NamedSourceCall = requires(Function&& function, Args&&... args) {
    ::nijigenerate::compat::detail::sourceCallableTarget(
        std::forward<Function>(function)).opCall(std::forward<Args>(args)...);
};

template<class T> struct SourceCallableClassHandle : std::false_type {};

template<class T> struct SourceCallableClassHandle<std::shared_ptr<T>>
    : std::bool_constant<::nicxlive::core::d_runtime::detail::sourceNativeObjectOrigin<T>> {};

template<class Receiver> requires (!SourceCallableClassHandle<std::remove_cvref_t<Receiver>>::value
    || requires(Receiver& receiver) { receiver.get(); })
constexpr auto sourceMemberReceiver(Receiver& receiver) noexcept {
    if constexpr (SourceCallableClassHandle<std::remove_cvref_t<Receiver>>::value) {
        auto* object = receiver.get();
        if constexpr (std::is_const_v<std::remove_reference_t<Receiver>>)
            return static_cast<std::add_const_t<std::remove_pointer_t<decltype(object)>>*>(object);
        else return object;
    } else return std::addressof(receiver);
}

template<class Receiver> using SourceMemberType = std::remove_pointer_t<
    decltype(detail::sourceMemberReceiver(std::declval<Receiver&>()))>;

template<class Function, class... Args> concept NamedSourceClassCall =
    SourceCallableClassHandle<std::remove_cvref_t<decltype(detail::sourceCallableTarget(
        std::declval<Function>()))>>::value
    && requires(Function&& function, Args&&... args) {
        detail::sourceCallableTarget(std::forward<Function>(function))->opCall(std::forward<Args>(args)...);
    };

template<class T> struct SourceCallableType;

template<class T> struct SourceCallableType<std::type_identity<T>> { using type = T; };

template<class Function> using SourceCallableTypeTarget = typename SourceCallableType<
    std::remove_cvref_t<decltype(detail::sourceCallableTarget(std::declval<Function>()))>>::type;

template<class Function, class... Args> concept NamedSourceTypeCall = requires(Args&&... args) {
    SourceCallableTypeTarget<Function>::opCall(std::forward<Args>(args)...);
};

template<class Function, class... Args> constexpr bool sourceCallNoexcept() {
    if constexpr (NamedSourceCall<Function, Args...>)
        return noexcept(::nijigenerate::compat::detail::sourceCallableTarget(
            std::declval<Function>()).opCall(std::declval<Args>()...));
    else if constexpr (NamedSourceClassCall<Function, Args...>)
        return noexcept(detail::sourceCallableTarget(std::declval<Function>())->opCall(std::declval<Args>()...));
    else if constexpr (NamedSourceTypeCall<Function, Args...>)
        return noexcept(SourceCallableTypeTarget<Function>::opCall(std::declval<Args>()...));
    else return std::is_nothrow_invocable_v<Function, Args...>;
}

template<class Function> constexpr bool sourcePreparationNoexcept() {
    using TargetReference = decltype(detail::sourceCallableTarget(std::declval<Function&>()));
    using Target = std::remove_cvref_t<TargetReference>;
    if constexpr (requires { SourceCallPreparation<Target>::prepare(std::declval<TargetReference>()); })
        return noexcept(SourceCallPreparation<Target>::prepare(std::declval<TargetReference>()));
    else return true;
}

} // namespace detail

// Source callers acquire the callee before evaluating argument expressions.
// Ordinary stored value callables remain references; only a declared alias
// acquisition supplies an actual pointer/handle snapshot.
template<class Function>
constexpr decltype(auto) ngPrepareSourceCallable(Function& function)
    noexcept(detail::sourcePreparationNoexcept<Function>()) {
    auto& target = detail::sourceCallableTarget(function);
    using Target = std::remove_cvref_t<decltype(target)>;
    if constexpr (requires { SourceCallPreparation<Target>::prepare(target); })
        return SourceCallPreparation<Target>::prepare(target);
    else return std::ref(target);
}

// The defining callable supplies its real opCall or native invocation lowering.
// No captured context, receiver, argument or result is materialized here.
template<class Function, class... Args>
    requires (detail::NamedSourceCall<Function, Args...> || detail::NamedSourceClassCall<Function, Args...>
        || detail::NamedSourceTypeCall<Function, Args...>
        || std::invocable<Function, Args...>)
constexpr decltype(auto) ngInvokeSourceCallable(Function&& function, Args&&... args)
    noexcept(detail::sourceCallNoexcept<Function, Args...>()) {
    if constexpr (detail::NamedSourceCall<Function, Args...>)
        return detail::sourceCallableTarget(std::forward<Function>(function)).opCall(std::forward<Args>(args)...);
    else if constexpr (detail::NamedSourceClassCall<Function, Args...>)
        return detail::sourceCallableTarget(std::forward<Function>(function))->opCall(std::forward<Args>(args)...);
    else if constexpr (detail::NamedSourceTypeCall<Function, Args...>)
        return detail::SourceCallableTypeTarget<Function>::opCall(std::forward<Args>(args)...);
    else return std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
}

struct SourceReferenceAliasAcquisition {
    template<class T> static constexpr auto acquire(T& value) noexcept { return std::ref(value); }
};

struct SourceFunctionPointerAliasAcquisition {
    template<class T> requires (std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>)
    static constexpr T acquire(T value) noexcept { return value; }
};

struct SourceClassHandleAliasAcquisition {
    template<class T> requires detail::SourceCallableClassHandle<std::shared_ptr<T>>::value
    static auto acquire(const std::shared_ptr<T>& value) noexcept { return value; }
};

// Existing captured-delegate publishers retain one callable context. Acquire
// that genuine descriptor before arguments, without cloning the callable.
template<class Signature> struct SourceCallPreparation<SharedDelegate<Signature>> {
    static SharedDelegate<Signature> prepare(const SharedDelegate<Signature>& value) noexcept { return value; }
};

// Acquisition is selected by the actual Source declaration, not native RTTI.
// The empty descriptor reads a mutable alias again for each Source call.
template<auto& Declaration, class Acquisition> struct SourceCallableAlias {};

template<auto& Declaration, class Acquisition>
struct SourceCallPreparation<SourceCallableAlias<Declaration, Acquisition>> {
    template<class Descriptor>
    static constexpr decltype(auto) prepare(Descriptor&) noexcept(noexcept(Acquisition::acquire(Declaration))) {
        return Acquisition::acquire(Declaration);
    }
};

template<auto& Declaration, class Acquisition>
constexpr auto ngSourceCallableAlias() { return SourceCallableAlias<Declaration, Acquisition>{}; }

// This signature comes from a Source declaration whose body has been lowered
// to reference cells. It is never inferred from a native callable taking T&.
// Byte cells cover Image ushort/float and explicitly declared unsigned Source
// char code units; native object cells additionally cover Image ubyte/CV int.
// A native char& body is not an unsigned Source body.
template<class Signature, class Body> class SourceReferenceCallable;

template<class Parameter> using SourceDeclaredArgument = std::conditional_t<
    std::is_lvalue_reference_v<Parameter>, SourceReference<std::remove_reference_t<Parameter>>,
    std::remove_cv_t<Parameter>>;

namespace detail {
template<class Result, class Actual> concept SourceDeclaredReturn = [] {
    if constexpr (std::is_lvalue_reference_v<Result>) {
        if constexpr (SourceByteReferenceElement<std::remove_reference_t<Result>>)
            return std::constructible_from<SourceReference<std::remove_reference_t<Result>>, Actual>;
        else return false;
    } else return std::same_as<Result, Actual>;
}();
}

template<class Parameter, class Argument>
    requires (std::is_lvalue_reference_v<Parameter> && requires(Argument&& argument) {
        ngBindSourceReference<std::remove_reference_t<Parameter>>(std::forward<Argument>(argument));
    })
SourceDeclaredArgument<Parameter> ngBindSourceDeclaredArgument(Argument&& argument) {
    return ngBindSourceReference<std::remove_reference_t<Parameter>>(std::forward<Argument>(argument));
}

template<class Parameter, class Argument>
    requires (!std::is_reference_v<Parameter> && requires(Argument&& argument) {
        ngBindSourcePrimitiveValue<std::remove_cv_t<Parameter>>(std::forward<Argument>(argument));
    })
SourceDeclaredArgument<Parameter> ngBindSourceDeclaredArgument(Argument&& argument) {
    return ngBindSourcePrimitiveValue<std::remove_cv_t<Parameter>>(std::forward<Argument>(argument));
}

template<class R, class... Parameters, class Body>
    requires ((!std::is_rvalue_reference_v<Parameters>
        && SourceByteReferenceElement<std::remove_reference_t<Parameters>>) && ...)
class SourceReferenceCallable<R(Parameters...), Body> {
    Body body_;

public:
    using SourceParameterSignature = R(Parameters...);
    explicit SourceReferenceCallable(Body body) : body_(std::forward<Body>(body)) {}

    template<std::size_t Index, class Argument>
        requires (Index < sizeof...(Parameters) && requires(Argument&& argument) {
            ngBindSourceDeclaredArgument<typename detail::SourceCallArgumentType<Index, Parameters...>::type>(
                std::forward<Argument>(argument));
        })
    static auto bindArgument(Argument&& argument)
        -> SourceDeclaredArgument<typename detail::SourceCallArgumentType<Index, Parameters...>::type> {
        using Parameter = typename detail::SourceCallArgumentType<Index, Parameters...>::type;
        return ngBindSourceDeclaredArgument<Parameter>(std::forward<Argument>(argument));
    }

    auto acquire() const {
        decltype(auto) acquired = ngPrepareSourceCallable(body_);
        return SourceReferenceCallable<R(Parameters...), decltype(acquired)>(
            std::forward<decltype(acquired)>(acquired));
    }

    // Reference parameters copy descriptors. Value parameters are the passive
    // primitive snapshots already captured by their ordered binding clauses.
    // A returned reference must designate a real native object or a retained
    // cell returned by the same body; a scalar value cannot become a reference.
    decltype(auto) opCall(SourceDeclaredArgument<Parameters>... arguments) const
        requires requires(const Body& body) {
            ngInvokeSourceCallable(body,
                std::declval<SourceDeclaredArgument<Parameters>>()...);
            requires detail::SourceDeclaredReturn<R, decltype(ngInvokeSourceCallable(body,
                std::declval<SourceDeclaredArgument<Parameters>>()...))>;
        } {
        if constexpr (std::is_lvalue_reference_v<R>) {
            using Cell = SourceReference<std::remove_reference_t<R>>;
            return Cell(ngInvokeSourceCallable(body_, std::move(arguments)...));
        } else {
            // Keep value-result elision and the body's actual exception path.
            return ngInvokeSourceCallable(body_, std::move(arguments)...);
        }
    }
};

template<class Signature, class Body>
struct SourceCallPreparation<SourceReferenceCallable<Signature, Body>> {
    static auto prepare(const SourceReferenceCallable<Signature, Body>& callable) {
        return callable.acquire();
    }
};

template<class SourceSignature, class Body>
auto ngSourceReferenceCallable(Body& body) {
    return SourceReferenceCallable<SourceSignature, std::reference_wrapper<Body>>(std::ref(body));
}

// Callers put each result directly into its own braced frame clause. A declared
// value parameter is loaded now, before the next argument expression executes.
// Unannotated auto-ref bodies keep their truthful original argument category.
template<std::size_t Index, class Function, class Argument>
    requires (!requires {
        typename std::remove_cvref_t<decltype(detail::sourceCallableTarget(std::declval<Function&>()))>
            ::SourceParameterSignature;
    } || requires(Function& function, Argument&& argument) {
        detail::sourceCallableTarget(function).template bindArgument<Index>(std::forward<Argument>(argument));
    })
decltype(auto) ngBindSourceCallArgument(Function& function, Argument&& argument) {
    auto& target = detail::sourceCallableTarget(function);
    if constexpr (requires { typename std::remove_cvref_t<decltype(target)>::SourceParameterSignature; })
        return target.template bindArgument<Index>(std::forward<Argument>(argument));
    else return std::forward<Argument>(argument);
}

} // namespace nijigenerate::compat
