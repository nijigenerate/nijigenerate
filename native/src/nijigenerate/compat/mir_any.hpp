#pragma once

// Native range/slice branch of Mir algorithm.iteration.any/anyImpl.
// Copyright 2020 Ilia Ki, Kaleidic Associates Advisory Limited,
// Symmetry Investments. Apache-2.0; see port/licenses/mir-iteration-any-LICENSE.txt.
// anyEmpty follows mir-core mir.primitives, Apache-2.0; authors Ilia Ki,
// Andrei Alexandrescu, David Simcha, and Jonathan M Davis.
// LightScopeOf follows mir-core mir.qualifier, Apache-2.0, author Ilia Ki.
// Nested array fuseShape follows mir.ndslice.fuse under the same Mir copyright
// and Apache-2.0 license, author Ilia Ki.
#include "assert_error.hpp"
#include "contiguous_view.hpp"
#include "byte_view.hpp"
#include "const_slice.hpp"
#include "retained_string_slice.hpp"
#include "source_bool.hpp"
#include "source_callable.hpp"
#include "source_call_arguments.hpp"
#include "phobos_ranges.hpp"
#include <common/shared_array.hpp>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat::mir {

// Defining slice publishers provide their actual source declaration metadata
// and flattened operation. Ordinary ranges are not inferred to be Mir slices.
template<class Range> struct SliceDeclaration;
template<class Range> struct StaticSliceDeclaration;
// Defining Source predicate mappings provide diagnostics for each actual
// instantiated range pack. Native callable type names are not Source aliases.
template<class Predicate> struct AnyPredicateDeclaration;

// A native iterator/size API alone does not establish Source range primitives.
// Alternate defining mappings provide these actual bodies explicitly.
template<class Range> struct SourceRangeOperations;

template<class T, std::size_t Rank, class Storage>
struct SliceDeclaration<ContiguousView<T, Rank, Storage>> {
    inline static constexpr bool contiguous = true;
    inline static constexpr std::size_t rank = Rank;
    static auto flattened(ContiguousView<T, Rank, Storage> input) {
        if constexpr (Rank == 1) return input;
        else {
            const auto count = input.elementCount();
            return input.template reshaped<1>({count});
        }
    }
};

namespace detail {
using ::nijigenerate::compat::detail::sourceMemberReceiver;
using ::nijigenerate::compat::detail::SourceMemberType;

// Aggregate cells retain parameter descriptors without tuple forwarding
// constructors. Traversal accesses each stored descriptor as an lvalue.
template<std::size_t Index, class... Ranges>
constexpr decltype(auto) rangeGet(SourceCallArguments<Ranges...>& ranges) noexcept {
    auto&& value = ngSourceCallArgument<Index>(ranges);
    return (value);
}

template<class Frame> struct RangeFrameSize;
template<class... Ranges> struct RangeFrameSize<SourceCallArguments<Ranges...>>
    : std::integral_constant<std::size_t, sizeof...(Ranges)> {};

template<class Function, class... Ranges, std::size_t... Index>
constexpr decltype(auto) applyRangeFrameImpl(Function&& function, SourceCallArguments<Ranges...>& ranges,
    std::index_sequence<Index...>) {
    return std::invoke(std::forward<Function>(function), detail::rangeGet<Index>(ranges)...);
}

template<class Function, class... Ranges>
constexpr decltype(auto) applyRangeFrame(Function&& function, SourceCallArguments<Ranges...>& ranges) {
    return detail::applyRangeFrameImpl(std::forward<Function>(function), ranges,
        std::index_sequence_for<Ranges...>{});
}

template<class... Ranges, std::size_t... Index>
auto copiedRangeFrame(SourceCallArguments<Ranges...>& ranges, std::index_sequence<Index...>) {
    return SourceCallArguments<std::remove_reference_t<Ranges>...>{
        {phobos::SourceValueOperations<std::remove_cvref_t<Ranges>>::copy(detail::rangeGet<Index>(ranges))}...};
}

template<class Value> auto sourceParameter(Value&& value) -> std::remove_cvref_t<Value> {
    using T = std::remove_cvref_t<Value>;
    if constexpr (std::is_lvalue_reference_v<Value>) return phobos::SourceValueOperations<T>::copy(value);
    else return phobos::SourceValueOperations<T>::transport(value);
}

template<class Expression>
auto sourceValueOf(Expression&& expression) -> std::remove_cvref_t<decltype(expression())> {
    using Value = decltype(expression());
    if constexpr (std::is_reference_v<Value>)
        return phobos::SourceValueOperations<std::remove_cvref_t<Value>>::copy(expression());
    else return expression();
}

template<class Range> using ConstSourceRange = std::add_const_t<std::remove_reference_t<Range>>;

struct Identity {
    // The actual default stringFun!"a" accepts 1..26 arguments and returns a.
    template<class T, class... Rest> requires (sizeof...(Rest) < 26)
    constexpr decltype(auto) operator()(T&& value, Rest&&...) const {
        if constexpr (std::is_lvalue_reference_v<T>) return std::forward<T>(value);
        else return std::remove_reference_t<T>(value);
    }
};
template<class Range> concept ShapeMethod = requires(Range& range) { sourceMemberReceiver(range)->shape(); };
template<class Range> concept ShapeProperty = ShapeMethod<Range>
    || requires(Range& range) { sourceMemberReceiver(range)->shape; };

template<ShapeProperty Range> decltype(auto) sourceShape(Range& range) {
    if constexpr (ShapeMethod<Range>) return sourceMemberReceiver(range)->shape();
    else return (sourceMemberReceiver(range)->shape);
}

template<class Range> concept FuseShapeMethod = requires(Range& range) {
    sourceMemberReceiver(range)->fuseShape();
};
template<class Range> concept FuseShapeProperty = FuseShapeMethod<Range>
    || requires(Range& range) { sourceMemberReceiver(range)->fuseShape; };

template<FuseShapeProperty Range> decltype(auto) sourceFuseShape(Range& range) {
    if constexpr (FuseShapeMethod<Range>) return sourceMemberReceiver(range)->fuseShape();
    else return (sourceMemberReceiver(range)->fuseShape);
}

template<class T> struct SourceFixedShape : std::integral_constant<std::size_t, 0> {
    inline static constexpr bool valid = false;
};

template<std::size_t Rank> struct SourceFixedShape<std::array<std::size_t, Rank>>
    : std::integral_constant<std::size_t, Rank> {
    inline static constexpr bool valid = true;
};

template<class T, class = void> struct SliceRank : std::integral_constant<std::size_t, 0> {
    inline static constexpr bool hasShape = false;
};
template<class T> struct SliceRank<T, std::void_t<
    decltype(detail::sourceShape(std::declval<const T&>()))>>
    : SourceFixedShape<std::remove_cvref_t<decltype(detail::sourceShape(std::declval<const T&>()))>> {
    inline static constexpr bool hasShape = SourceFixedShape<std::remove_cvref_t<
        decltype(detail::sourceShape(std::declval<const T&>()))>>::valid;
};
template<class T> inline constexpr std::size_t sliceRank = SliceRank<std::remove_cvref_t<T>>::value;
template<class T> inline constexpr bool hasRankedShape = SliceRank<std::remove_cvref_t<T>>::hasShape;
template<class T> inline constexpr std::size_t sourceDimensionCount = hasRankedShape<T> ? sliceRank<T> : 1;

template<class Range> concept LengthMethod = requires(Range& range) {
    { sourceMemberReceiver(range)->length() } -> std::convertible_to<std::size_t>;
};
template<class Range> concept LengthProperty = LengthMethod<Range> || requires(Range& range) {
    { sourceMemberReceiver(range)->length } -> std::convertible_to<std::size_t>;
};
template<class Range> struct SourceArrayCarrier;

template<class Range> concept NativeSizeMember = SourceArrayCarrier<std::remove_cvref_t<Range>>::value
    && requires(Range& range) {
    { sourceMemberReceiver(range)->size() } -> std::convertible_to<std::size_t>;
};
template<class Range> concept DeclaredLength = requires(Range& range) {
    { SourceRangeOperations<std::remove_cvref_t<Range>>::length(range) } -> std::convertible_to<std::size_t>;
};

template<class Range> concept MappedLength = LengthProperty<Range> || NativeSizeMember<Range> || DeclaredLength<Range>;

template<class Range> requires MappedLength<const Range>
std::size_t sourceLength(const Range& range) {
    // Mir hasLength and its shape fallback read a const descriptor.
    if constexpr (LengthMethod<const Range>) return sourceMemberReceiver(range)->length();
    else if constexpr (LengthProperty<const Range>) return sourceMemberReceiver(range)->length;
    else if constexpr (NativeSizeMember<const Range>) return sourceMemberReceiver(range)->size();
    else return SourceRangeOperations<std::remove_cvref_t<Range>>::length(range);
}

template<class T> inline constexpr bool sourceCharacter =
    std::same_as<std::remove_cv_t<T>, char> || std::same_as<std::remove_cv_t<T>, char8_t>
    || std::same_as<std::remove_cv_t<T>, char16_t> || std::same_as<std::remove_cv_t<T>, char32_t>
    || std::same_as<std::remove_cv_t<T>, wchar_t>;

template<class Range> struct SourceArrayCarrier : std::false_type {};
template<class T> struct SourceArrayCarrier<Slice<T>> : std::true_type {};
template<class T> struct SourceArrayCarrier<ConstSlice<T>> : std::true_type {};
template<class T> struct SourceArrayCarrier<ByteView<T>> : std::true_type {};
template<class T> struct SourceArrayCarrier<nicxlive::core::common::SharedArray<T>> : std::true_type {};
template<> struct SourceArrayCarrier<RetainedString> : std::true_type {};
template<> struct SourceArrayCarrier<RetainedStringSlice> : std::true_type {};

template<class Range> struct SourceArrayElement;
template<class T> struct SourceArrayElement<Slice<T>> { using type = T; };
template<class T> struct SourceArrayElement<ConstSlice<T>> { using type = const T; };
template<class T> struct SourceArrayElement<ByteView<T>> { using type = T; };
template<class T> struct SourceArrayElement<nicxlive::core::common::SharedArray<T>> { using type = T; };
template<> struct SourceArrayElement<RetainedString> { using type = const char; };
template<> struct SourceArrayElement<RetainedStringSlice> { using type = const char; };

template<class Range> concept SourceArray = SourceArrayCarrier<std::remove_cvref_t<Range>>::value;

template<SourceArray Range> decltype(auto) arrayCell(Range& range, std::size_t index) {
    using Element = typename SourceArrayElement<std::remove_cvref_t<Range>>::type;
    using Value = typename phobos::detail::SourceArrayValue<Element>::type;
    if constexpr (std::same_as<std::remove_const_t<Value>, char8_t>)
        return SourceReference<Value>::fromCodeUnitsUnchecked(range, index);
    else if constexpr (requires { range.unchecked(index); }) return range.unchecked(index);
    else return range.data()[index];
}

// The defining Slice mapping has DeepElement T. Scalars terminate fusion;
// genuine nested Slice declarations recursively contribute their dimensions.
// Other shaped/length-bearing elements still require their actual provider.
template<class T, class = void> struct DeclaredFuseDimensions : std::integral_constant<std::size_t, 0> {
    inline static constexpr bool available = !ShapeProperty<const T> && !MappedLength<const T>;
};

template<class T> requires FuseShapeProperty<T>
struct DeclaredFuseDimensions<T, std::void_t<decltype(detail::sourceFuseShape(std::declval<T&>()))>>
    : SourceFixedShape<std::remove_cvref_t<decltype(detail::sourceFuseShape(std::declval<T&>()))>> {
    using Shape = SourceFixedShape<std::remove_cvref_t<decltype(detail::sourceFuseShape(std::declval<T&>()))>>;
    inline static constexpr bool available = Shape::valid && (ShapeProperty<T> || MappedLength<const T>);
};

template<class T> struct ContiguousFuseDimensions : DeclaredFuseDimensions<T> {};

template<SourceArray Range> struct ContiguousFuseDimensions<Range>
    : std::integral_constant<std::size_t,
        sourceCharacter<typename SourceArrayElement<std::remove_cvref_t<Range>>::type> ? 0
            : 1 + ContiguousFuseDimensions<typename SourceArrayElement<std::remove_cvref_t<Range>>::type>::value> {
    using Element = typename SourceArrayElement<std::remove_cvref_t<Range>>::type;
    inline static constexpr bool available = sourceCharacter<Element> || ContiguousFuseDimensions<Element>::available;
};

[[noreturn]] void throwFuseShapeException();

template<SourceArray Range>
    requires (ContiguousFuseDimensions<Range>::available
        && !sourceCharacter<typename SourceArrayElement<std::remove_cvref_t<Range>>::type>)
auto sourceFuseShape(Range range);

template<class T, std::size_t Rank, class Storage>
struct ContiguousFuseDimensions<ContiguousView<T, Rank, Storage>>
    : std::integral_constant<std::size_t,
        Rank == 1 && sourceCharacter<T> ? 0 : Rank + ContiguousFuseDimensions<T>::value> {
    inline static constexpr bool available = Rank > 0
        && ((Rank == 1 && sourceCharacter<T>) || ContiguousFuseDimensions<T>::available);
};

template<class T, std::size_t Rank, class Storage>
struct ContiguousFuseDimensions<const ContiguousView<T, Rank, Storage>>
    : std::integral_constant<std::size_t, ContiguousFuseDimensions<ContiguousView<T, Rank, Storage>>::value> {
    using Declaration = ContiguousFuseDimensions<ContiguousView<T, Rank, Storage>>;
    inline static constexpr bool available = Declaration::available
        && (Declaration::value == Rank || (Rank == 1 && sourceCharacter<T>));
};

template<class T, std::size_t Rank, class Storage>
    requires (ContiguousFuseDimensions<ContiguousView<T, Rank, Storage>>::available
        && !(Rank == 1 && sourceCharacter<T>))
auto sourceFuseShape(ContiguousView<T, Rank, Storage> range) {
    constexpr auto fusedRank = ContiguousFuseDimensions<ContiguousView<T, Rank, Storage>>::value;
    if constexpr (fusedRank == Rank) return range.shape();
    else {
        std::array<std::size_t, fusedRank> result{};
        const auto dimensions = range.shape();
        bool nonempty = true;
        for (std::size_t i = 0; i < Rank; ++i) {
            result[i] = dimensions[i];
            if (!dimensions[i]) nonempty = false;
        }
        if (nonempty) {
            // Mir Slice.first dereferences its genuine raw iterator; it does
            // not inspect every element or perform a second array bounds check.
            auto& first = range.uncheckedElement(0);
            const auto nested = detail::sourceFuseShape(first);
            for (std::size_t i = 0; i < nested.size(); ++i) result[Rank + i] = nested[i];
        }
        return result;
    }
}

template<SourceArray Range>
    requires (ContiguousFuseDimensions<Range>::available
        && !sourceCharacter<typename SourceArrayElement<std::remove_cvref_t<Range>>::type>)
auto sourceFuseShape(Range range) {
    constexpr auto rank = ContiguousFuseDimensions<Range>::value;
    std::array<std::size_t, rank> result{};
    const auto count = range.size();
    result[0] = count;
    if constexpr (rank > 1) {
        if (!count) return result;
        bool next = false;
        auto* const elements = range.data();
        // The original array descriptor is retained by value, while each
        // recursive element is the actual array cell, not a copied element.
        for (std::size_t index = 0; index < count; ++index) {
            const auto nested = detail::sourceFuseShape(elements[index]);
            if (next) {
                bool equal = true;
                for (std::size_t axis = 0; axis < nested.size(); ++axis)
                    if (result[axis + 1] != nested[axis]) { equal = false; break; }
                if (!equal) detail::throwFuseShapeException();
            } else {
                for (std::size_t axis = 0; axis < nested.size(); ++axis) result[axis + 1] = nested[axis];
                next = true;
            }
        }
    }
    return result;
}

template<class Range> constexpr bool declaredContiguousSlice() {
    using Declaration = SliceDeclaration<std::remove_cvref_t<Range>>;
    if constexpr (requires { Declaration::contiguous; Declaration::rank; }) return Declaration::contiguous;
    else return false;
}

template<class... Ranges> constexpr bool allContiguousSlices() {
    if constexpr (sizeof...(Ranges) && (declaredContiguousSlice<Ranges>() && ...)) {
        constexpr auto rank = SliceDeclaration<std::remove_cvref_t<
            std::tuple_element_t<0, std::tuple<Ranges...>>>>::rank;
        return rank > 1 && ((SliceDeclaration<std::remove_cvref_t<Ranges>>::rank == rank) && ...);
    } else return false;
}
template<class R> concept SourceEmptyMember = requires(R& range) {
    { ngSourceBool(sourceMemberReceiver(range)->empty()) } -> std::same_as<bool>;
} || requires(R& range) {
    { ngSourceBool(sourceMemberReceiver(range)->empty) } -> std::same_as<bool>;
};

template<SourceEmptyMember R> decltype(auto) sourceEmptyValue(R& range) {
    if constexpr (requires {
        { ngSourceBool(sourceMemberReceiver(range)->empty()) } -> std::same_as<bool>;
    })
        return sourceMemberReceiver(range)->empty();
    else return (sourceMemberReceiver(range)->empty);
}

template<SourceEmptyMember R> bool sourceEmpty(R& range) {
    return ngSourceBool(detail::sourceEmptyValue(range));
}

template<class R> concept SourceFrontMethod = requires(R& range) {
    requires (!std::is_void_v<decltype(sourceMemberReceiver(range)->front())>);
};

template<class R> concept SourceFrontMember = SourceFrontMethod<R> || requires(R& range) {
    sourceMemberReceiver(range)->front;
    requires (!std::is_void_v<decltype((sourceMemberReceiver(range)->front))>);
};

template<SourceFrontMember R> decltype(auto) sourceFront(R& range) {
    if constexpr (SourceFrontMethod<R>) return sourceMemberReceiver(range)->front();
    else return (sourceMemberReceiver(range)->front);
}

template<class R> concept SourcePopFrontMember = requires(R& range) {
    sourceMemberReceiver(range)->popFront();
};

template<SourcePopFrontMember R> void sourcePopFront(R& range) { sourceMemberReceiver(range)->popFront(); }

template<class R> concept SourceRange = SourceEmptyMember<R> && SourceFrontMember<R> && SourcePopFrontMember<R>;


template<class Range> concept DeclaredTraversal = requires(Range& range) {
    SourceRangeOperations<std::remove_cvref_t<Range>>::cursor(range);
    SourceRangeOperations<std::remove_cvref_t<Range>>::end(range);
};

template<class Range> concept FlatTraversable = SourceRange<Range>
    || SourceArrayCarrier<std::remove_cvref_t<Range>>::value || DeclaredTraversal<Range>;

template<class... Ranges> constexpr bool mixedMemberIteratorRanges() {
    return (FlatTraversable<Ranges> && ...)
        && (SourceRange<Ranges> || ...) && !(SourceRange<Ranges> && ...);
}

template<class Range> bool anyEmpty(Range& range) {
    if constexpr (requires { sourceMemberReceiver(range)->anyEmpty(); }) {
        if constexpr (std::same_as<std::remove_cvref_t<decltype(sourceMemberReceiver(range)->anyEmpty())>, bool>)
            return sourceMemberReceiver(range)->anyEmpty();
        else return SourceRangeOperations<std::remove_cvref_t<Range>>::anyEmpty(range);
    } else if constexpr (requires { sourceMemberReceiver(range)->anyEmpty; }) {
        if constexpr (std::same_as<std::remove_cvref_t<decltype((sourceMemberReceiver(range)->anyEmpty))>, bool>)
            return sourceMemberReceiver(range)->anyEmpty;
        else return SourceRangeOperations<std::remove_cvref_t<Range>>::anyEmpty(range);
    } else if constexpr (ShapeProperty<Range>) {
        const auto dimensions = detail::sourceShape(range);
        static_assert(SourceFixedShape<std::remove_cvref_t<decltype(dimensions)>>::valid,
            "Source anyEmptyShape requires the genuine fixed size_t array");
        for (const auto length : dimensions) if (length == 0) return true;
        return false;
    } else if constexpr (SourceEmptyMember<Range>) {
        static_assert(std::same_as<std::remove_cvref_t<decltype(detail::sourceEmptyValue(range))>, bool>,
            "The unshaped Source anyEmpty overload requires a genuine bool empty result");
        return detail::sourceEmptyValue(range);
    } else return SourceRangeOperations<std::remove_cvref_t<Range>>::empty(range);
}

inline void checkShape(bool condition, std::size_t sourceLine = 610) {
#ifndef NDEBUG
    if (!condition) throw ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("all slices must have the same shape"),
        RetainedString::fromStaticLiteral("mir/algorithm/iteration.d"), sourceLine);
#else
    (void)condition;
    (void)sourceLine;
#endif
}

template<class Predicate, class... Ranges>
void checkDeclaredShape(bool condition, std::size_t sourceLine = 610) {
#ifndef NDEBUG
    if (!condition) {
        if constexpr (sizeof...(Ranges) > 1 && requires {
            { AnyPredicateDeclaration<std::remove_cvref_t<Predicate>>::template shapeErrorMessage<Ranges...>() }
                -> std::same_as<RetainedString>;
        }) {
            throw ngStaticError<AssertError>(
                AnyPredicateDeclaration<std::remove_cvref_t<Predicate>>::template shapeErrorMessage<Ranges...>(),
                RetainedString::fromStaticLiteral("mir/algorithm/iteration.d"), sourceLine);
        } else static_assert(sizeof...(Ranges) == 0,
            "The Source shape assertion requires its defining alias and static range-origin producer");
    }
#else
    (void)condition;
    (void)sourceLine;
#endif
}
template<class Range, std::size_t Rank, std::size_t... I>
decltype(auto) element(Range& range, const std::array<std::size_t, Rank>& coordinate, std::index_sequence<I...>) {
    if constexpr (hasRankedShape<Range>) return sourceMemberReceiver(range)->at(coordinate[I]...);
    else {
        static_assert(Rank == 1);
        if constexpr (SourceArrayCarrier<std::remove_cvref_t<Range>>::value) {
            return detail::arrayCell(range, coordinate[0]);
        } else return SourceRangeOperations<std::remove_cvref_t<Range>>::element(range, coordinate[0]);
    }
}
template<std::size_t Rank, class Range>
std::array<std::size_t, Rank> shape(Range& range) {
    if constexpr (hasRankedShape<Range>) {
        static_assert(sliceRank<Range> == Rank, "Fused-rank Mir slice mapping is not yet supplied");
        const auto lengths = detail::sourceShape(range);
        std::array<std::size_t, Rank> result{};
        for (std::size_t i = 0; i < Rank; ++i) result[i] = lengths[i];
        return result;
    } else {
        static_assert(Rank == 1 && MappedLength<const Range>);
        return {detail::sourceLength(range)};
    }
}
template<class Range> decltype(auto) sourceLightScope(Range& range) {
    if constexpr (requires { sourceMemberReceiver(range)->lightScope(); })
        return sourceMemberReceiver(range)->lightScope();
    else if constexpr ((std::is_const_v<Range> || std::is_const_v<SourceMemberType<Range>>)
        && requires { sourceMemberReceiver(range)->lightConst(); })
        return sourceMemberReceiver(range)->lightConst();
    else return (range);
}

template<class Range> constexpr bool changesLightScope() {
    return !std::same_as<std::remove_reference_t<decltype(detail::sourceLightScope(std::declval<Range&>()))>,
        std::remove_reference_t<Range>>;
}

template<class Range> decltype(auto) lightScope(Range& range) {
    // allLightScope aliases dynamic arrays and unchanged types directly; only
    // a changed LightScopeOf type evaluates the actual conversion property.
    if constexpr (!SourceArrayCarrier<std::remove_cvref_t<Range>>::value && changesLightScope<Range>())
        return detail::sourceLightScope(range);
    else return (range);
}

template<class Range> inline constexpr std::size_t originalShapeRank = [] {
    if constexpr (ShapeProperty<Range>) {
        using Shape = SourceFixedShape<std::remove_cvref_t<
            decltype(detail::sourceShape(std::declval<Range&>()))>>;
        static_assert(Shape::valid, "Source original shape requires a fixed size_t array");
        return Shape::value;
    } else {
        static_assert(MappedLength<ConstSourceRange<Range>>,
            "Source original shape requires its actual shape or length provider");
        return std::size_t(1);
    }
}();

template<std::size_t Rank, class Range>
std::array<std::size_t, Rank> originalShape(Range& range) {
    constexpr auto sourceRank = originalShapeRank<Range>;
    if constexpr (sourceRank != Rank) {
        const auto dimensions = detail::sourceFuseShape(range);
        using FusedShape = SourceFixedShape<std::remove_cvref_t<decltype(dimensions)>>;
        static_assert(FusedShape::valid, "Source fuseShape returns a fixed size_t array");
        static_assert(FusedShape::value >= Rank,
            "Source fuseShape must supply at least the first input's dimensions");
        std::array<std::size_t, Rank> result{};
        for (std::size_t i = 0; i < Rank; ++i) result[i] = dimensions[i];
        return result;
    } else if constexpr (ShapeProperty<Range>) {
        const auto dimensions = detail::sourceShape(range);
        std::array<std::size_t, Rank> result{};
        for (std::size_t i = 0; i < Rank; ++i) result[i] = dimensions[i];
        return result;
    } else {
        static_assert(Rank == 1 && MappedLength<const Range>);
        return {detail::sourceLength(range)};
    }
}

template<class Predicate = void, class... Ranges>
void checkOriginalShapes(SourceCallArguments<Ranges...>& ranges) {
    if constexpr (sizeof...(Ranges) > 1) {
        constexpr auto rank = originalShapeRank<std::tuple_element_t<0, std::tuple<Ranges...>>>;
        // Source checkShapesMatch receives its own by-value parameter pack.
        auto copied = detail::copiedRangeFrame(ranges, std::index_sequence_for<Ranges...>{});
        detail::applyRangeFrame([&](auto& first, auto&... rest) {
            const auto check = [&](auto& range) {
                constexpr auto otherRank = originalShapeRank<std::remove_reference_t<decltype(range)>>;
                if constexpr (otherRank != rank) {
                    using FusedShape = SourceFixedShape<std::remove_cvref_t<
                        decltype(detail::sourceFuseShape(range))>>;
                    static_assert(FusedShape::valid, "Source fuseShape returns a fixed size_t array");
                    static_assert(FusedShape::value >= rank,
                        "Source fuseShape must supply at least the first input's dimensions");
                }
#ifndef NDEBUG
                const auto left = originalShape<rank>(range);
                const auto right = originalShape<rank>(first);
                checkDeclaredShape<Predicate, std::remove_reference_t<Ranges>...>(
                    left == right, otherRank == rank ? 610 : 615);
#else
                (void)range;
#endif
            };
            (check(rest), ...);
#ifdef NDEBUG
            (void)first;
#endif
        }, copied);
    }
}

template<class Range, std::size_t Rank, class Indices> struct SliceFrontExpression;
template<class Range, std::size_t Rank, std::size_t... I>
struct SliceFrontExpression<Range, Rank, std::index_sequence<I...>> {
    Range& range;
    const std::array<std::size_t, Rank>& coordinate;
    decltype(auto) operator()() const { return element(range, coordinate, std::index_sequence<I...>{}); }
    decltype(auto) reference() const
        requires requires { sourceMemberReceiver(range)->sourceReferenceAt(coordinate[I]...); } {
        return sourceMemberReceiver(range)->sourceReferenceAt(coordinate[I]...);
    }
    auto reference() const requires (Rank == 1 && !hasRankedShape<Range>
        && SourceArrayCarrier<std::remove_cvref_t<Range>>::value
        && SourceByteReferenceElement<typename phobos::detail::SourceArrayValue<
            typename SourceArrayElement<std::remove_cvref_t<Range>>::type>::type>) {
        decltype(auto) cell = operator()();
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else return SourceReference<std::remove_reference_t<decltype(cell)>>::fromRetainedObjectCell(
            range.retentionOwner(), cell);
    }
};

template<std::size_t Index, class Predicate, class Ranges, std::size_t Rank>
decltype(auto) boundSliceFront(Predicate& target, Ranges& ranges,
    const std::array<std::size_t, Rank>& coordinate) {
    auto& range = detail::rangeGet<Index>(ranges);
    return phobos::detail::bindFrontExpression<Index>(target,
        SliceFrontExpression<std::remove_reference_t<decltype(range)>, Rank,
            std::make_index_sequence<Rank>>{range, coordinate});
}

template<class Predicate, class Ranges, std::size_t Rank, std::size_t... I>
bool invokeSlices(Predicate& predicate, Ranges& ranges, const std::array<std::size_t, Rank>& coordinate,
    std::index_sequence<I...>) {
    decltype(auto) target = ::nijigenerate::compat::ngPrepareSourceCallable(predicate);
    // Braced initialization evaluates source fronts left to right. References
    // stay references; mutable predicates write the genuine backing elements.
    SourceCallArguments<decltype(boundSliceFront<I>(target, ranges, coordinate))...> values{
        {boundSliceFront<I>(target, ranges, coordinate)}...};
    return ngSourceBool(::nijigenerate::compat::ngInvokeSourceCallable(std::forward<decltype(target)>(target),
        ngSourceCallArgument<I>(values)...));
}
template<class Predicate, class Ranges, std::size_t Rank>
bool visitSlices(Predicate& predicate, Ranges& ranges, const std::array<std::size_t, Rank>& dimensions,
    std::array<std::size_t, Rank>& coordinate, std::size_t dimension) {
    if (dimension == Rank)
        return invokeSlices(predicate, ranges, coordinate,
            std::make_index_sequence<RangeFrameSize<Ranges>::value>{});
    for (std::size_t i = 0; i < dimensions[dimension]; ++i) {
        coordinate[dimension] = i;
        if (visitSlices(predicate, ranges, dimensions, coordinate, dimension + 1)) return true;
    }
    return false;
}
template<class Iterator> struct IteratorFrontExpression {
    Iterator& input;
    decltype(auto) operator()() const { return *input; }
    decltype(auto) reference() const requires requires { input.sourceReference(); } { return input.sourceReference(); }
};
template<std::size_t Index, class Predicate, class Iterators>
decltype(auto) boundIteratorFront(Predicate& target, Iterators& iterators) {
    return phobos::detail::bindFrontExpression<Index>(target,
        IteratorFrontExpression{detail::rangeGet<Index>(iterators)});
}

template<class Predicate, class Iterators, std::size_t... I>
bool invokeRanges(Predicate& predicate, Iterators& iterators, std::index_sequence<I...>) {
    decltype(auto) target = ::nijigenerate::compat::ngPrepareSourceCallable(predicate);
    SourceCallArguments<decltype(boundIteratorFront<I>(target, iterators))...> values{
        {boundIteratorFront<I>(target, iterators)}...};
    return ngSourceBool(::nijigenerate::compat::ngInvokeSourceCallable(std::forward<decltype(target)>(target),
        ngSourceCallArgument<I>(values)...));
}

struct SourceTraversalEnd {};

template<class Range> class ArrayTraversalCursor {
public:
    explicit ArrayTraversalCursor(Range& range) : range_(range), length_(range.size()) {}
    decltype(auto) operator*() {
        if (index_ == length_) {
#ifndef NDEBUG
            throw ngStaticError<AssertError>(RetainedString::fromStaticLiteral("Accessing front of an empty array."),
                RetainedString::fromStaticLiteral("mir/primitives.d"), 198);
#else
            throw ngStaticError<ArrayIndexError>(0, 0,
                RetainedString::fromStaticLiteral("mir/primitives.d"), 199);
#endif
        }
        return detail::arrayCell(range_, index_);
    }
    auto sourceReference() requires SourceByteReferenceElement<
        typename phobos::detail::SourceArrayValue<typename SourceArrayElement<std::remove_cvref_t<Range>>::type>::type> {
        decltype(auto) cell = **this;
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else {
            using Value = std::remove_reference_t<decltype(cell)>;
            return SourceReference<Value>::fromRetainedObjectCell(range_.retentionOwner(), cell);
        }
    }
    ArrayTraversalCursor& operator++() {
        if (index_ == length_) {
#ifndef NDEBUG
            throw ngStaticError<AssertError>(
                RetainedString::fromStaticLiteral("Evaluating popFront() on an empty array."),
                RetainedString::fromStaticLiteral("mir/primitives.d"), 229);
#else
            throw ngStaticError<ArraySliceError>(1, 0, 0,
                RetainedString::fromStaticLiteral("mir/primitives.d"), 230);
#endif
        }
        ++index_;
        return *this;
    }
    bool operator!=(SourceTraversalEnd) const { return index_ != length_; }
private:
    Range& range_;
    std::size_t length_, index_ = 0;
};

// This cursor refers to the original frame descriptor; it owns no range or data.
template<SourceRange Range> class SourceTraversalCursor {
public:
    explicit SourceTraversalCursor(Range& range) : range_(range) {}
    decltype(auto) operator*() { return detail::sourceFront(range_); }
    decltype(auto) sourceReference()
        requires requires { sourceMemberReceiver(range_)->sourceReferenceFront(); } {
        return sourceMemberReceiver(range_)->sourceReferenceFront();
    }
    SourceTraversalCursor& operator++() {
        detail::sourcePopFront(range_);
        return *this;
    }
    bool operator!=(SourceTraversalEnd) { return !detail::sourceEmpty(range_); }
private:
    Range& range_;
};

template<class Range> auto traversalCursor(Range& range) {
    if constexpr (SourceRange<Range>) return SourceTraversalCursor<Range>(range);
    else if constexpr (SourceArrayCarrier<std::remove_cvref_t<Range>>::value)
        return ArrayTraversalCursor<Range>(range);
    else return SourceRangeOperations<std::remove_cvref_t<Range>>::cursor(range);
}

template<class Range> auto traversalEnd(Range& range) {
    if constexpr (SourceRange<Range> || SourceArrayCarrier<std::remove_cvref_t<Range>>::value)
        return SourceTraversalEnd{};
    else return SourceRangeOperations<std::remove_cvref_t<Range>>::end(range);
}
template<class Iterators, std::size_t... I>
void popRanges(Iterators& iterators, std::index_sequence<I...>) {
    // Mir pops the last range first after every unsuccessful predicate call.
    ((void)++detail::rangeGet<sizeof...(I) - 1 - I>(iterators), ...);
}
template<class Range> struct MemberFrontExpression {
    Range& input;
    decltype(auto) operator()() const { return detail::sourceFront(input); }
    decltype(auto) reference() const
        requires requires { sourceMemberReceiver(input)->sourceReferenceFront(); } {
        return sourceMemberReceiver(input)->sourceReferenceFront();
    }
};
template<std::size_t Index, class Predicate, class Ranges>
decltype(auto) boundMemberFront(Predicate& target, Ranges& ranges) {
    return phobos::detail::bindFrontExpression<Index>(target,
        MemberFrontExpression{detail::rangeGet<Index>(ranges)});
}

template<class Predicate, class Ranges, std::size_t... I>
bool invokeSourceRanges(Predicate& predicate, Ranges& ranges, std::index_sequence<I...>) {
    decltype(auto) target = ::nijigenerate::compat::ngPrepareSourceCallable(predicate);
    SourceCallArguments<decltype(boundMemberFront<I>(target, ranges))...> values{
        {boundMemberFront<I>(target, ranges)}...};
    // A value-returning source front remains an rvalue, so a ref-only predicate
    // cannot accidentally mutate a synthetic temporary as if it were backing.
    return ngSourceBool(::nijigenerate::compat::ngInvokeSourceCallable(std::forward<decltype(target)>(target),
        ngSourceCallArgument<I>(values)...));
}
template<class Ranges, std::size_t... I>
void popSourceRanges(Ranges& ranges, std::index_sequence<I...>) {
    ((void)detail::sourcePopFront(detail::rangeGet<sizeof...(I) - 1 - I>(ranges)), ...);
}
}

namespace detail {
template<bool Scoped, class Predicate, class... Ranges> requires (sizeof...(Ranges) > 0)
bool anyImpl(Predicate& predicate, SourceCallArguments<Ranges...>& ranges);

template<class Range> decltype(auto) flattenedLightScope(Range& input) {
    using Scoped = std::remove_cvref_t<decltype(detail::lightScope(input))>;
    return SliceDeclaration<Scoped>::flattened(detail::lightScope(input));
}

template<class Predicate, class... Ranges>
bool withFlattenedLightScopes(Predicate& predicate, SourceCallArguments<Ranges...>& ranges) {
    return detail::applyRangeFrame([&](auto&... input) {
        // Each real scope conversion precedes its corresponding flattening.
        // The original parameters retain all backing throughout recursive any.
        SourceCallArguments<std::remove_reference_t<decltype(detail::flattenedLightScope(input))>...> flattened{
            {detail::sourceValueOf([&]() -> decltype(auto) { return detail::flattenedLightScope(input); })}...};
        return detail::anyImpl<false>(predicate, flattened);
    }, ranges);
}

template<class Predicate, class Ranges, std::size_t... I>
bool recurseSourceSlices(Predicate& predicate, Ranges& ranges, std::index_sequence<I...>) {
    // frontOf2 returns each genuine lower-dimensional descriptor by value.
    // Braced initialization preserves its left-to-right evaluation order.
    SourceCallArguments<std::remove_reference_t<decltype(detail::sourceFront(detail::rangeGet<I>(ranges)))>...> fronts{
        {detail::sourceValueOf([&]() -> decltype(auto) {
            return detail::sourceFront(detail::rangeGet<I>(ranges));
        })}...};
    return detail::anyImpl<true>(predicate, fronts);
}

template<class Predicate, class Cursors, std::size_t... I>
bool recurseTraversalSlices(Predicate& predicate, Cursors& cursors, std::index_sequence<I...>) {
    SourceCallArguments<std::remove_reference_t<decltype(*detail::rangeGet<I>(cursors))>...> fronts{
        {detail::sourceValueOf([&]() -> decltype(auto) { return *detail::rangeGet<I>(cursors); })}...};
    return detail::anyImpl<true>(predicate, fronts);
}

template<class Predicate, class... Ranges>
bool withLightScopes(Predicate& predicate, SourceCallArguments<Ranges...>& ranges) {
    return detail::applyRangeFrame([&](auto&... input) {
        // Each direct initialization supplies the by-value anyImpl parameter.
        SourceCallArguments<std::remove_reference_t<decltype(detail::lightScope(input))>...> scoped{
            {detail::sourceValueOf([&]() -> decltype(auto) { return detail::lightScope(input); })}...};
        return detail::anyImpl<true>(predicate, scoped);
    }, ranges);
}
}

// Canonical callable lowering of Mir's naryFun alias. No model/pixel copies or
// std::any_of substitution; input descriptors are copied as the source does.
template<bool Scoped, class Predicate, class... Ranges> requires (sizeof...(Ranges) > 0)
bool detail::anyImpl(Predicate& predicate, SourceCallArguments<Ranges...>& ranges) {
    // Selected Mir 3.22.4 tests an undeclared S in both is(S : bit-Slice)
    // branches. Those is-expressions are false; do not substitute Slices[0]
    // or import a different release's accelerator into this traversal body.
    if constexpr (!Scoped && detail::allContiguousSlices<Ranges...>()) {
        detail::checkOriginalShapes<Predicate>(ranges);
        return detail::withFlattenedLightScopes(predicate, ranges);
    } else if constexpr (!Scoped) {
        detail::checkOriginalShapes<Predicate>(ranges);
        if (detail::anyEmpty(detail::rangeGet<0>(ranges))) return false;
        return detail::withLightScopes(predicate, ranges);
    } else if constexpr ((detail::SourceRange<Ranges> && ...)) {
        constexpr auto rank = detail::sourceDimensionCount<std::tuple_element_t<0, std::tuple<Ranges...>>>;
        do {
            if constexpr (rank == 1) {
                if (detail::invokeSourceRanges(predicate, ranges, std::index_sequence_for<Ranges...>{})) return true;
            } else {
                if (detail::recurseSourceSlices(predicate, ranges, std::index_sequence_for<Ranges...>{})) return true;
            }
            detail::popSourceRanges(ranges, std::index_sequence_for<Ranges...>{});
        } while (!detail::sourceEmpty(detail::rangeGet<0>(ranges)));
        return false;
    } else if constexpr ((detail::hasRankedShape<Ranges> || ...)
        && !detail::mixedMemberIteratorRanges<Ranges...>()) {
        constexpr std::size_t rank = [] {
            std::size_t result = 0;
            bool found = false;
            ([&] {
                if constexpr (detail::hasRankedShape<Ranges>) {
                    if (!found) { result = detail::sliceRank<Ranges>; found = true; }
                }
            }(), ...);
            return result;
        }();
        const auto dimensions = detail::shape<rank>(detail::rangeGet<0>(ranges));
        std::array<std::size_t, rank> coordinate{};
        return detail::visitSlices(predicate, ranges, dimensions, coordinate, 0);
    } else {
        static_assert((detail::FlatTraversable<Ranges> && ...));
        auto iterators = detail::applyRangeFrame([](auto&... range) {
            return SourceCallArguments<decltype(detail::traversalCursor(range))...>{
                {detail::traversalCursor(range)}...};
        }, ranges);
        const auto end = detail::traversalEnd(detail::rangeGet<0>(ranges));
        do {
            if constexpr (detail::sourceDimensionCount<std::tuple_element_t<0, std::tuple<Ranges...>>> != 1) {
                if (detail::recurseTraversalSlices(predicate, iterators,
                    std::index_sequence_for<Ranges...>{})) return true;
            } else {
                if (detail::invokeRanges(predicate, iterators, std::index_sequence_for<Ranges...>{})) return true;
            }
            detail::popRanges(iterators, std::index_sequence_for<Ranges...>{});
        } while (detail::rangeGet<0>(iterators) != end);
        return false;
    }
}

namespace detail {
template<class Predicate, class... Ranges> requires (sizeof...(Ranges) > 0)
bool anyEntry(Predicate& predicate, Ranges&&... input) {
    // Materialize exactly the public Source parameter frame. Defining value
    // operations perform actual postblits; native wrappers add no value frames.
    SourceCallArguments<std::remove_cvref_t<Ranges>...> ranges{
        {detail::sourceParameter(std::forward<Ranges>(input))}...};
    return detail::anyImpl<false>(predicate, ranges);
}
}

template<class Predicate, class... Ranges> requires (sizeof...(Ranges) > 0)
bool ngAny(Predicate&& predicate, Ranges&&... input) {
    return detail::anyEntry(predicate, std::forward<Ranges>(input)...);
}

template<auto Predicate = detail::Identity{}> struct Any {
    template<class... Ranges> requires (sizeof...(Ranges) > 0)
    bool operator()(Ranges&&... ranges) const {
        PhobosPredicateAlias<Predicate> predicate;
        return detail::anyEntry(predicate, std::forward<Ranges>(ranges)...);
    }
    template<class RuntimePredicate, class... Ranges> requires (sizeof...(Ranges) > 0)
    bool with(RuntimePredicate&& predicate, Ranges&&... ranges) const {
        return detail::anyEntry(predicate, std::forward<Ranges>(ranges)...);
    }
};
} // namespace nijigenerate::compat::mir
