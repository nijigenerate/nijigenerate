#pragma once

#include "mir_any.hpp"
#include "source_function_origin.hpp"
#include "source_static_function_origin.hpp"
#include "source_delegate.hpp"
#include <concepts>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {

// Select these only from the actual Source naryFun branch. The native body
// cannot determine whether its Source alias was a string, symbol or opCall.
struct MirStringPredicateAlias {
    using StaticAlias = StaticMirStringFunctionAlias;
    static SourceFunctionOwnedArgument argument() { return ngMirStringFunctionPredicateArgument(); }
};
struct MirStaticOpCallPredicateAlias {
    using StaticAlias = StaticMirOpCallAlias;
    static SourceFunctionOwnedArgument argument() { return ngMirStaticOpCallPredicateArgument(); }
};

template<class Alias> concept SourceMirPredicateAlias = requires {
    { Alias::argument() } -> std::same_as<SourceFunctionOwnedArgument>;
};

template<SourceMirPredicateAlias Alias, class Callable> struct SourceMirPredicate {
    Callable function;
    template<class... Args> requires requires(Callable& body, Args&&... args) {
        ngInvokeSourceCallable(ngPrepareSourceCallable(body), std::forward<Args>(args)...);
    }
    constexpr decltype(auto) opCall(Args&&... args)
        noexcept(noexcept(ngInvokeSourceCallable(ngPrepareSourceCallable(function), std::forward<Args>(args)...))) {
        decltype(auto) target = ngPrepareSourceCallable(function);
        return ngInvokeSourceCallable(std::forward<decltype(target)>(target), std::forward<Args>(args)...);
    }
    template<class... Args> requires requires(const Callable& body, Args&&... args) {
        ngInvokeSourceCallable(ngPrepareSourceCallable(body), std::forward<Args>(args)...);
    }
    constexpr decltype(auto) opCall(Args&&... args) const
        noexcept(noexcept(ngInvokeSourceCallable(ngPrepareSourceCallable(function), std::forward<Args>(args)...))) {
        decltype(auto) target = ngPrepareSourceCallable(function);
        return ngInvokeSourceCallable(std::forward<decltype(target)>(target), std::forward<Args>(args)...);
    }
};

namespace source_mir_detail {
template<class Prepared, class = void> struct ParameterSignature {};
template<class Prepared> struct ParameterSignature<Prepared, std::void_t<
    typename std::remove_cvref_t<decltype(detail::sourceCallableTarget(std::declval<Prepared&>()))>
        ::SourceParameterSignature>> {
    using SourceParameterSignature = typename std::remove_cvref_t<
        decltype(detail::sourceCallableTarget(std::declval<Prepared&>()))>::SourceParameterSignature;
};
}

// Preparation has already acquired this exact callee before the caller reads
// any fronts. Do not prepare it again after those argument expressions run.
template<SourceMirPredicateAlias Alias, class Prepared>
struct PreparedSourceMirPredicate : source_mir_detail::ParameterSignature<Prepared> {
    Prepared function;
    template<std::size_t Index, class Argument>
        requires requires(Prepared& target, Argument&& argument) {
            ngBindSourceCallArgument<Index>(target, std::forward<Argument>(argument));
        }
    decltype(auto) bindArgument(Argument&& argument) {
        return ngBindSourceCallArgument<Index>(function, std::forward<Argument>(argument));
    }
    template<class... Args> requires requires(Prepared& target, Args&&... args) {
        ngInvokeSourceCallable(std::forward<Prepared>(target), std::forward<Args>(args)...);
    }
    decltype(auto) opCall(Args&&... args)
        noexcept(noexcept(ngInvokeSourceCallable(std::forward<Prepared>(function), std::forward<Args>(args)...))) {
        return ngInvokeSourceCallable(std::forward<Prepared>(function), std::forward<Args>(args)...);
    }
    template<class... Args> requires requires(const Prepared& target, Args&&... args) {
        ngInvokeSourceCallable(std::forward<const Prepared>(target), std::forward<Args>(args)...);
    }
    decltype(auto) opCall(Args&&... args) const
        noexcept(noexcept(ngInvokeSourceCallable(std::forward<const Prepared>(function), std::forward<Args>(args)...))) {
        return ngInvokeSourceCallable(std::forward<const Prepared>(function), std::forward<Args>(args)...);
    }
};

template<SourceMirPredicateAlias Alias, class Callable>
constexpr auto ngSourceMirPredicate(Callable&& body) {
    // A named declaring body is not a new Source-owned callable value. Its
    // enclosing context supplies lifetime; escaping bindings need that actual
    // retention program, not a copied native closure with copied Source fields.
    if constexpr (std::is_lvalue_reference_v<Callable>)
        return SourceMirPredicate<Alias, std::reference_wrapper<std::remove_reference_t<Callable>>>{std::ref(body)};
    else return SourceMirPredicate<Alias, std::decay_t<Callable>>{std::forward<Callable>(body)};
}

template<class Alias, class Callable>
struct SourceCallPreparation<SourceMirPredicate<Alias, Callable>> {
    template<class Bound> static constexpr auto prepare(Bound& predicate) {
        decltype(auto) target = ngPrepareSourceCallable(predicate.function);
        // Retain Source alias metadata while acquiring the native target before
        // the caller evaluates its fronts. Ordinary bodies remain references.
        return PreparedSourceMirPredicate<Alias, decltype(target)>{{}, std::forward<decltype(target)>(target)};
    }
};

} // namespace nijigenerate::compat

namespace nijigenerate::compat::phobos {

// Metadata adds no Source-owned callable value. Retain the actual defining
// alias binding; arbitrary C++ references do not establish escaping lifetime.
template<class Alias, class Callable>
struct SourcePredicateBinding<SourceMirPredicate<Alias, Callable>> {
    using Predicate = SourceMirPredicate<Alias, Callable>;
    using Binding = SourcePredicateBinding<Callable>;
    static Predicate initialize() { return {Binding::initialize()}; }
    static Predicate copy(Predicate& source) { return {Binding::copy(source.function)}; }
    static Predicate copy(const Predicate& source)
        requires requires { Binding::copy(source.function); } { return {Binding::copy(source.function)}; }
    static Predicate transport(Predicate& source) { return {Binding::transport(source.function)}; }
    static void assign(Predicate& destination, Predicate& source) {
        Binding::assign(destination.function, source.function);
    }
    static void swap(Predicate& left, Predicate& right)
        noexcept(noexcept(Binding::swap(left.function, right.function))) {
        Binding::swap(left.function, right.function);
    }
};

} // namespace nijigenerate::compat::phobos

namespace nijigenerate::compat::mir {

namespace detail {
template<class Element> struct StaticArraySliceDeclaration {
    static consteval auto unqualifiedName() {
        constexpr auto element = phobos::SourceArrayElementSpelling<Element>::get();
        source_static_detail::Text<element.size() + 2> result;
        result.append(element);
        result.append("[]");
        return result;
    }
};
struct StaticStringSliceDeclaration {
    static consteval auto unqualifiedName() {
        source_static_detail::Text<6> result;
        result.append("string");
        return result;
    }
};
}

template<class T> struct StaticSliceDeclaration<Slice<T>> {
    using type = detail::StaticArraySliceDeclaration<T>;
};
template<class T> struct StaticSliceDeclaration<ConstSlice<T>> {
    using type = detail::StaticArraySliceDeclaration<const T>;
};
template<ReinterpretedByteElement T> struct StaticSliceDeclaration<ByteView<T>> {
    using type = detail::StaticArraySliceDeclaration<T>;
};
template<class T> struct StaticSliceDeclaration<nicxlive::core::common::SharedArray<T>> {
    using type = detail::StaticArraySliceDeclaration<T>;
};
template<> struct StaticSliceDeclaration<RetainedString> { using type = detail::StaticStringSliceDeclaration; };
template<> struct StaticSliceDeclaration<RetainedStringSlice> : StaticSliceDeclaration<RetainedString> {};

template<class Alias, class Callable>
struct AnyPredicateDeclaration<SourceMirPredicate<Alias, Callable>> {
    template<class... Ranges> requires (sizeof...(Ranges) > 1)
    static RetainedString shapeErrorMessage() {
        // Each recursive public any invocation supplies its own real range
        // identities, including lower-rank and flattened Slice declarations.
        if constexpr (requires { typename Alias::StaticAlias; }
            && (requires { typename StaticSliceDeclaration<std::remove_cvref_t<Ranges>>::type; } && ...)) {
            return ngStaticMirAnyShapeErrorMessage<typename Alias::StaticAlias,
                typename StaticSliceDeclaration<std::remove_cvref_t<Ranges>>::type...>();
        } else static_assert(sizeof...(Ranges) == 0,
            "The defining Source alias and range declarations must supply their actual static diagnostic origins");
    }
};

// This internal default represents the defining Mir stringFun!"a" branch;
// arbitrary native callables do not acquire a Source alias from their type.
template<> struct AnyPredicateDeclaration<detail::Identity>
    : AnyPredicateDeclaration<SourceMirPredicate<MirStringPredicateAlias, detail::Identity>> {};

template<> struct AnyPredicateDeclaration<PhobosPredicateAlias<detail::Identity{}>>
    : AnyPredicateDeclaration<detail::Identity> {};

} // namespace nijigenerate::compat::mir
