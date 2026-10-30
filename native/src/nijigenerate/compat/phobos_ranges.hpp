#pragma once

// Native source-range branches of Phobos std.algorithm.iteration FilterResult
// and UniqResult. Copyright Andrei Alexandrescu 2008-. BSL-1.0; see
// port/licenses/phobos-optimum-sum.txt (same upstream module and license).
// Array equality follows core.internal.array.equality, Copyright Digital Mars
// 2000-2020, under the same Boost Software License 1.0.
#include "slice.hpp"
#include "const_slice.hpp"
#include "byte_view.hpp"
#include "retained_string_slice.hpp"
#include "phobos_unicode_cursor.hpp"
#include "assert_error.hpp"
#include "source_bool.hpp"
#include "source_callable.hpp"
#include <compat/object_metadata.hpp>
#include <math/types.hpp>
#include <common/shared_array.hpp>
#include <resource.hpp>
#include <common/object_key_policy.hpp>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {
template<class Signature, class Context> class SourceDelegate;
// Bind the actual template alias, rather than cloning a callable object into
// every range value. The type includes the alias value as Source does.
template<auto Predicate, bool = std::is_class_v<decltype(Predicate)>> struct PhobosPredicateAlias;
template<auto Predicate> struct PhobosPredicateAlias<Predicate, true> {
    static constexpr decltype(auto) target() noexcept { return (Predicate); }
};
template<auto Predicate> struct PhobosPredicateAlias<Predicate, false> {
    inline static constexpr auto value = Predicate;
    static constexpr decltype(auto) target() noexcept { return (value); }
};
template<auto Predicate> struct SourceCallPreparation<PhobosPredicateAlias<Predicate>> {
    template<class Descriptor> static decltype(auto) prepare(Descriptor&)
        noexcept(noexcept(ngPrepareSourceCallable(PhobosPredicateAlias<Predicate>::target()))) {
        return ngPrepareSourceCallable(PhobosPredicateAlias<Predicate>::target());
    }
};
}

namespace nijigenerate::compat::phobos {
template<class Cursor, class Predicate> class FilterResult;
template<class Cursor, class Predicate> class UniqResult;

// Defining value types provide their actual Source initializer, postblit copy,
// assignment and elided-return transport. Missing records remain missing; native
// constructibility and nominal type-name records do not supply these operations.
// The returned carrier's destructor must implement that defining Source value's
// lifetime. In particular, transport must not introduce another Source postblit.
// A transferred-from carrier must not later repeat the transferred logical
// Source destructor. Its native rvalue parameter transport must follow the same
// rule, or its defining callable must supply the actual signature lowering.
// Destructor-bearing records also supply the selected D ABI's
// retireAssignedArgument(owner): retire now for callee-owned arguments, or retain
// the actual owner in a real caller-expression frame for caller-owned arguments.
// They also supply constructorFailure(owner) inside the active exception handler,
// applying the selected Source constructor's Exception/Throwable and field-destruction policy
// before native field cleanup. A native catch-all is not that Source policy.
template<class T, class = void> struct SourceValueOperations;

// The Source predicate is a template alias, not another owned field of the
// range struct. Its native binding retains the defining alias/context without
// invoking a Source value postblit or cloning a callable object.
template<class Predicate> struct SourcePredicateBinding;

// Defining records preserve Source qualifiers erased by native scalar/proxy types.
template<class Range> struct SourceRangeValueDeclaration;
struct SourceStringCodeUnit {};

// Array assertions contain a compile-time T.stringof literal in Phobos.
// Defining nonprimitive elements provide that actual spelling, independently
// of runtime metadata callbacks and their allocation/exception behavior.
template<class T, class = void> struct SourceArrayElementSpelling {
    static consteval std::string_view get() {
        using Origin = ::nicxlive::core::d_runtime::SourceTypeSpellings<T>;
        using Text = decltype(Origin::unqualifiedElement);
        static_assert(std::is_array_v<Text>
            && std::same_as<std::remove_cv_t<std::remove_extent_t<Text>>, char>,
            "The defining Source element needs its actual static unqualified spelling");
        return std::string_view(Origin::unqualifiedElement);
    }
};

template<class T> struct SourceArrayElementSpelling<T,
    std::enable_if_t<std::is_arithmetic_v<T> && std::same_as<T, std::remove_cv_t<T>>>> {
    static consteval std::string_view get() {
        if constexpr (std::same_as<T, bool>) return "bool";
        else if constexpr (std::same_as<T, char> || std::same_as<T, char8_t>) return "char";
        else if constexpr (std::same_as<T, char16_t> || (std::same_as<T, wchar_t> && sizeof(T) == 2))
            return "wchar";
        else if constexpr (std::same_as<T, char32_t> || std::same_as<T, wchar_t>) return "dchar";
        else if constexpr (std::same_as<T, float>) return "float";
        else if constexpr (std::same_as<T, double>) return "double";
        else if constexpr (std::same_as<T, long double>) return "real";
        else if constexpr (sizeof(T) == 1) return std::is_signed_v<T> ? "byte" : "ubyte";
        else if constexpr (sizeof(T) == 2) return std::is_signed_v<T> ? "short" : "ushort";
        else if constexpr (sizeof(T) == 4) return std::is_signed_v<T> ? "int" : "uint";
        else if constexpr (sizeof(T) == 8) return std::is_signed_v<T> ? "long" : "ulong";
        else {
            static_assert(sizeof(T) == 16, "The Source primitive declaration needs its actual spelling");
            return std::is_signed_v<T> ? "cent" : "ucent";
        }
    }
};

template<class T> struct SourceArrayElementSpelling<const T> {
    inline static constexpr auto bytes = [] {
        constexpr auto name = SourceArrayElementSpelling<T>::get();
        std::array<char, name.size() + 8> result{};
        std::size_t i = 0;
        for (const auto byte : std::string_view("const(")) result[i++] = byte;
        for (const auto byte : name) result[i++] = byte;
        result[i] = ')';
        return result;
    }();
    static consteval std::string_view get() { return {bytes.data(), bytes.size() - 1}; }
};

template<> struct SourceArrayElementSpelling<nicxlive::core::math::Vec2> {
    static consteval std::string_view get() { return "Vector!(float, 2)"; }
};
template<> struct SourceArrayElementSpelling<nicxlive::core::math::Vec3> {
    static consteval std::string_view get() { return "Vector!(float, 3)"; }
};
template<> struct SourceArrayElementSpelling<nicxlive::core::math::Vec4> {
    static consteval std::string_view get() { return "Vector!(float, 4)"; }
};

template<> struct SourceArrayElementSpelling<RetainedString> {
    static consteval std::string_view get() { return "string"; }
};
template<> struct SourceArrayElementSpelling<RetainedStringSlice> : SourceArrayElementSpelling<RetainedString> {};
template<> struct SourceArrayElementSpelling<SourceStringCodeUnit> {
    static consteval std::string_view get() { return "immutable(char)"; }
};

namespace detail {
template<class Element> struct DynamicArrayElementSpelling {
    inline static constexpr auto bytes = [] {
        constexpr auto element = SourceArrayElementSpelling<Element>::get();
        std::array<char, element.size() + 3> result{};
        std::size_t i = 0;
        for (const auto byte : element) result[i++] = byte;
        result[i++] = '[';
        result[i] = ']';
        return result;
    }();
    static consteval std::string_view get() { return {bytes.data(), bytes.size() - 1}; }
};

template<class T> struct DescriptorValueOperations {
    using SourceArrayAssignmentProgram = DescriptorValueOperations;
    inline static constexpr bool hasDestructor = false;
    inline static constexpr bool hasPostblit = false;
    static T initialize() { return {}; }
    static T copy(const T& value) { return value; }
    static T transport(T& value) { return value; }
    template<class Value> static T transport(const Value& value) requires std::same_as<Value, T> { return value; }
    static void assign(T& destination, const T& source) { destination = source; }
    static void swap(T& left, T& right) noexcept(noexcept(std::swap(left, right))) { std::swap(left, right); }
    template<std::size_t Count, class Array> static void assignStaticArray(Array& destination, Array& source) {
        [[maybe_unused]] const auto previous = destination;
        for (std::size_t i = 0; i < Count; ++i) assign(destination[i], source[i]);
    }
};
}

template<auto Predicate> struct SourcePredicateBinding<PhobosPredicateAlias<Predicate>>
    : detail::DescriptorValueOperations<PhobosPredicateAlias<Predicate>> {};

template<auto& Declaration, class Acquisition>
struct SourcePredicateBinding<SourceCallableAlias<Declaration, Acquisition>>
    : detail::DescriptorValueOperations<SourceCallableAlias<Declaration, Acquisition>> {};

template<class Signature, class Context>
struct SourcePredicateBinding<SourceDelegate<Signature, Context>>
    : detail::DescriptorValueOperations<SourceDelegate<Signature, Context>> {};

template<class T> struct SourceArrayElementSpelling<Slice<T>> : detail::DynamicArrayElementSpelling<T> {};
template<class T> struct SourceArrayElementSpelling<ConstSlice<T>> : detail::DynamicArrayElementSpelling<const T> {};
template<ReinterpretedByteElement T> struct SourceArrayElementSpelling<ByteView<T>>
    : detail::DynamicArrayElementSpelling<T> {};
template<class T> struct SourceArrayElementSpelling<nicxlive::core::common::SharedArray<T>>
    : detail::DynamicArrayElementSpelling<T> {};

// These are actual Source dynamic-array descriptors, never copies of elements.
template<class T> struct SourceValueOperations<Slice<T>> : detail::DescriptorValueOperations<Slice<T>> {};
template<class T> struct SourceValueOperations<ConstSlice<T>> : detail::DescriptorValueOperations<ConstSlice<T>> {};

template<ReinterpretedByteElement T> struct SourceValueOperations<ByteView<T>>
    : detail::DescriptorValueOperations<ByteView<T>> {};
template<class T> struct SourceValueOperations<nicxlive::core::common::SharedArray<T>>
    : detail::DescriptorValueOperations<nicxlive::core::common::SharedArray<T>> {};
template<> struct SourceValueOperations<RetainedString> : detail::DescriptorValueOperations<RetainedString> {};
template<> struct SourceValueOperations<RetainedStringSlice> : detail::DescriptorValueOperations<RetainedStringSlice> {};
template<auto Predicate> struct SourceValueOperations<PhobosPredicateAlias<Predicate>>
    : detail::DescriptorValueOperations<PhobosPredicateAlias<Predicate>> {};
template<auto& Declaration, class Acquisition>
struct SourceValueOperations<SourceCallableAlias<Declaration, Acquisition>>
    : detail::DescriptorValueOperations<SourceCallableAlias<Declaration, Acquisition>> {};
template<class Signature, class Context>
struct SourceValueOperations<SourceDelegate<Signature, Context>>
    : detail::DescriptorValueOperations<SourceDelegate<Signature, Context>> {};

// A genuine class reference has no value postblit: retain the same actual object.
template<class T> requires (::nicxlive::core::d_runtime::detail::sourceNativeObjectOrigin<T>)
struct SourceValueOperations<std::shared_ptr<T>> : detail::DescriptorValueOperations<std::shared_ptr<T>> {};

template<class T> struct SourceValueOperations<T, std::enable_if_t<
    std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_same_v<T, std::nullptr_t>>> {
    using SourceArrayAssignmentProgram = SourceValueOperations;
    inline static constexpr bool hasDestructor = false;
    inline static constexpr bool hasPostblit = false;
    static T initialize() {
        using Value = std::remove_cv_t<T>;
        if constexpr (std::is_floating_point_v<Value>) return std::numeric_limits<Value>::quiet_NaN();
        else if constexpr (std::same_as<Value, char> || std::same_as<Value, char8_t>) return static_cast<T>(0xff);
        else if constexpr (std::same_as<Value, char16_t> || std::same_as<Value, char32_t>
            || std::same_as<Value, wchar_t>)
            return static_cast<T>(0xffff);
        else return {};
    }
    static T copy(T value) { return value; }
    static T transport(T& value) { return value; }
    template<class Value> static T transport(const Value& value) requires std::same_as<Value, T> { return value; }
    static void assign(T& destination, T source) { destination = source; }
    static void swap(T& left, T& right) noexcept { std::swap(left, right); }
    template<std::size_t Count, class Array> static void assignStaticArray(Array& destination, Array& source) {
        for (std::size_t i = 0; i < Count; ++i) assign(destination[i], source[i]);
    }
};

// A named enum copies its declared value. Its potentially nonzero .init needs
// the defining enum producer; it cannot be inferred from native E{}.
template<class T> struct SourceValueOperations<T, std::enable_if_t<std::is_enum_v<T>>> {
    using SourceArrayAssignmentProgram = SourceValueOperations;
    inline static constexpr bool hasDestructor = false;
    inline static constexpr bool hasPostblit = false;
    static T copy(T value) { return value; }
    static T transport(T& value) { return value; }
    template<class Value> static T transport(const Value& value) requires std::same_as<Value, T> { return value; }
    static void assign(T& destination, T source) { destination = source; }
    static void swap(T& left, T& right) noexcept { std::swap(left, right); }
    template<std::size_t Count, class Array> static void assignStaticArray(Array& destination, Array& source) {
        for (std::size_t i = 0; i < Count; ++i) assign(destination[i], source[i]);
    }
};

template<> struct SourceValueOperations<nicxlive::core::math::Vec2>
    : detail::DescriptorValueOperations<nicxlive::core::math::Vec2> {
    static nicxlive::core::math::Vec2 initialize() {
        const auto nan = SourceValueOperations<float>::initialize();
        return {nan, nan};
    }
};
template<> struct SourceValueOperations<nicxlive::core::math::Vec3>
    : detail::DescriptorValueOperations<nicxlive::core::math::Vec3> {
    static nicxlive::core::math::Vec3 initialize() {
        const auto nan = SourceValueOperations<float>::initialize();
        return {nan, nan, nan};
    }
};
template<> struct SourceValueOperations<nicxlive::core::math::Vec4>
    : detail::DescriptorValueOperations<nicxlive::core::math::Vec4> {
    static nicxlive::core::math::Vec4 initialize() {
        const auto nan = SourceValueOperations<float>::initialize();
        return {nan, nan, nan, nan};
    }
};

namespace detail {
template<class Element, class Self, bool Empty, class = void> struct ArrayAssignmentProgram {};
template<class Element, class Self> struct ArrayAssignmentProgram<Element, Self, true> {
    using SourceArrayAssignmentProgram = Self;
};
template<class Element, class Self>
struct ArrayAssignmentProgram<Element, Self, false, std::void_t<typename Element::SourceArrayAssignmentProgram>> {
    using SourceArrayAssignmentProgram = Self;
};
}

// Source static arrays initialize and postblit actual elements in index order.
// Nested aggregate clauses directly initialize cells without wrapper temporaries.
template<class T, std::size_t N> struct SourceValueOperations<std::array<T, N>>
    : detail::ArrayAssignmentProgram<SourceValueOperations<std::remove_cv_t<T>>,
        SourceValueOperations<std::array<T, N>>, N == 0> {
    using Array = std::array<T, N>;
    using Element = SourceValueOperations<std::remove_cv_t<T>>;
    inline static constexpr bool hasDestructor = [] {
        if constexpr (N == 0) return false;
        else return Element::hasDestructor;
    }();
    inline static constexpr bool hasPostblit = [] {
        if constexpr (N == 0) return false;
        else return Element::hasPostblit;
    }();
    inline static constexpr bool passiveSwap = [] {
        if constexpr (N == 0) return true;
        else return noexcept(Element::swap(std::declval<T&>(), std::declval<T&>()));
    }();
    static Array initialize() {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return Array{{((void)I, Element::initialize())...}};
        }(std::make_index_sequence<N>{});
    }
    template<class Source> static Array copy(Source& source)
        requires (std::same_as<std::remove_const_t<Source>, Array>
            && (N == 0 || (hasDestructor && requires { Element::template copyStaticArray<N>(source); })
                || (!hasDestructor && requires { Element::copy(source[0]); }))) {
        if constexpr (hasDestructor) {
            // Selected core.internal.array.construction catches Exception for
            // prefix cleanup. Native aggregate catch-all destruction is not
            // that policy for destructor-bearing elements.
            return Element::template copyStaticArray<N>(source);
        } else {
            return [&]<std::size_t... I>(std::index_sequence<I...>) {
                return Array{{Element::copy(source[I])...}};
            }(std::make_index_sequence<N>{});
        }
    }
    static Array transport(Array& source) {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return Array{{Element::transport(source[I])...}};
        }(std::make_index_sequence<N>{});
    }
    static void assign(Array& destination, Array& source)
        requires (!std::is_const_v<T>
            && (N == 0 || requires { Element::template assignStaticArray<N>(destination, source); })) {
        // Array assignment uses the selected arrayassign/blit program, not
        // arbitrary user element opAssign calls. Intrinsic defining records
        // above supply their actual callback-free array bodies.
        if constexpr (N != 0) Element::template assignStaticArray<N>(destination, source);
    }
    template<std::size_t Count, class Outer> static void assignStaticArray(Outer& destination, Outer& source)
        requires ((N == 0 || requires { typename Element::SourceArrayAssignmentProgram; })
            && requires(Array& left, Array& right) { assign(left, right); }) {
        [[maybe_unused]] const auto previous = destination;
        for (std::size_t i = 0; i < Count; ++i) assign(destination[i], source[i]);
    }
    static void swap(Array& left, Array& right) noexcept(passiveSwap) {
        if constexpr (N != 0)
            for (std::size_t i = 0; i < N; ++i) Element::swap(left[i], right[i]);
    }
};

namespace detail {
using ::nijigenerate::compat::detail::sourceMemberReceiver;
using ::nijigenerate::compat::detail::SourceMemberType;

template<class Callee> using BoundCallee = std::remove_cvref_t<decltype(
    ::nijigenerate::compat::detail::sourceCallableTarget(std::declval<Callee&>()))>;
template<class Callee> concept DeclaredParameters = requires { typename BoundCallee<Callee>::SourceParameterSignature; };

template<class Expression> decltype(auto) declaredFront(Expression& expression) {
    if constexpr (requires { expression.reference(); }) return expression.reference();
    else return expression();
}

template<std::size_t Index, class Callee, class Expression>
    requires (!DeclaredParameters<Callee> || requires(Callee& callee, Expression& expression) {
        ngBindSourceCallArgument<Index>(callee, declaredFront(expression));
    })
decltype(auto) bindFrontExpression(Callee& callee, Expression&& expression) {
    auto& target = ::nijigenerate::compat::detail::sourceCallableTarget(callee);
    if constexpr (requires { typename std::remove_cvref_t<decltype(target)>::SourceParameterSignature; })
        return ngBindSourceCallArgument<Index>(callee, declaredFront(expression));
    // A forwarding-reference binder would turn a value front into T&& and
    // leave its argument temporary dead before invocation. Return it directly.
    else return expression();
}

template<class R> concept EmptyMethod = requires(R& range) {
    requires std::same_as<std::remove_reference_t<decltype(sourceMemberReceiver(range)->empty())>, bool>;
};

template<class R> concept StaticEmpty = requires {
    requires std::same_as<std::remove_cv_t<decltype(R::empty)>, bool>;
    typename std::bool_constant<R::empty>;
};

template<class R> concept StaticEmptyMethod = requires {
    requires std::same_as<std::remove_cv_t<decltype(R::empty())>, bool>;
    typename std::bool_constant<R::empty()>;
};

template<class R> concept EmptyProperty = EmptyMethod<R> || StaticEmpty<SourceMemberType<R>> || requires(R& range) {
    requires std::same_as<std::remove_reference_t<decltype((sourceMemberReceiver(range)->empty))>, bool>;
};

template<EmptyProperty R> bool readEmpty(R& range) {
    if constexpr (EmptyMethod<R>) return sourceMemberReceiver(range)->empty();
    else return sourceMemberReceiver(range)->empty;
}

template<class R> concept FrontMethod = requires(R& range) {
    requires (!std::is_void_v<decltype(sourceMemberReceiver(range)->front())>);
};

template<class R> concept FrontProperty = FrontMethod<R> || requires(R& range) {
    sourceMemberReceiver(range)->front;
    requires (!std::is_void_v<decltype((sourceMemberReceiver(range)->front))>);
};

template<FrontProperty R> decltype(auto) readFront(R& range) {
    if constexpr (FrontMethod<R>) return sourceMemberReceiver(range)->front();
    else return (sourceMemberReceiver(range)->front);
}

template<class R> concept BackMethod = requires(R& range) {
    requires (!std::is_void_v<decltype(sourceMemberReceiver(range)->back())>);
};

template<class R> concept BackProperty = BackMethod<R> || requires(R& range) {
    sourceMemberReceiver(range)->back;
    requires (!std::is_void_v<decltype((sourceMemberReceiver(range)->back))>);
};

template<BackProperty R> decltype(auto) readBack(R& range) {
    if constexpr (BackMethod<R>) return sourceMemberReceiver(range)->back();
    else return (sourceMemberReceiver(range)->back);
}

template<class R> concept SaveMethod = requires(R& range) { sourceMemberReceiver(range)->save(); };
template<class R> concept SaveProperty = SaveMethod<R> || requires(R& range) { sourceMemberReceiver(range)->save; };

template<SaveProperty R> decltype(auto) readSave(R& range) {
    if constexpr (SaveMethod<R>) return sourceMemberReceiver(range)->save();
    else return (sourceMemberReceiver(range)->save);
}

template<class R> concept PopFrontMethod = requires(R& range) {
    sourceMemberReceiver(range)->popFront();
};

template<PopFrontMethod R> void popFront(R& range) { sourceMemberReceiver(range)->popFront(); }

template<class R> concept PopBackMethod = requires(R& range) { sourceMemberReceiver(range)->popBack(); };

template<PopBackMethod R> void popBack(R& range) { sourceMemberReceiver(range)->popBack(); }

template<class R> concept MemberRange = EmptyProperty<R> && FrontProperty<R> && PopFrontMethod<R>;
template<class R> concept SourceForwardRange = MemberRange<R> && SaveProperty<R> && requires(R& range) {
    requires std::same_as<std::remove_reference_t<decltype(detail::readSave(range))>, R>;
};
// Records bind genuine Source dynamic-array carriers. An indexed native struct
// alone is not an array declaration or an isInputRange proof.
template<class R> struct IndexedRangeTraits;
template<class T> struct SourceArrayValue { using type = T; };
template<> struct SourceArrayValue<char> { using type = char8_t; };
template<> struct SourceArrayValue<const char> { using type = const char8_t; };

template<class T> struct IndexedRangeTraits<Slice<T>> {
    using value_type = typename SourceArrayValue<T>::type;
    using source_element_type = T;
};

template<class T> struct IndexedRangeTraits<ConstSlice<T>> {
    using value_type = typename SourceArrayValue<const T>::type;
    using source_element_type = const T;
};

template<ReinterpretedByteElement T> struct IndexedRangeTraits<ByteView<T>> {
    using value_type = T;
    using source_element_type = T;
};
template<class T> struct IndexedRangeTraits<nicxlive::core::common::SharedArray<T>> {
    using value_type = typename SourceArrayValue<T>::type;
    using source_element_type = T;
};

template<> struct IndexedRangeTraits<RetainedString> {
    using value_type = const char8_t;
    using source_element_type = SourceStringCodeUnit;
};
template<> struct IndexedRangeTraits<RetainedStringSlice> : IndexedRangeTraits<RetainedString> {};

enum class ArrayOperation { Front, Back, PopFront, PopBack };

template<ArrayOperation Operation> consteval std::string_view arrayMessagePrefix() {
    if constexpr (Operation == ArrayOperation::Front) return "Attempting to fetch the front of an empty array of ";
    else if constexpr (Operation == ArrayOperation::Back) return "Attempting to fetch the back of an empty array of ";
    else if constexpr (Operation == ArrayOperation::PopFront) return "Attempting to popFront() past the end of an array of ";
    else return "Attempting to popBack() past the front of an array of ";
}

template<class R, ArrayOperation Operation, bool Autodecoded = false>
RetainedString indexedArrayMessage() noexcept {
    // Unicode overloads accept const(T)[] / inout(C)[] and deduce the
    // unqualified code unit; the plain T[] overload retains element const.
    using DeclaredElement = typename IndexedRangeTraits<R>::source_element_type;
    using Element = std::conditional_t<Autodecoded,
        std::conditional_t<std::same_as<DeclaredElement, SourceStringCodeUnit>, char,
            std::remove_const_t<DeclaredElement>>, DeclaredElement>;
    static constexpr auto bytes = [] {
        constexpr auto prefix = arrayMessagePrefix<Operation>();
        constexpr auto name = SourceArrayElementSpelling<Element>::get();
        std::array<char, prefix.size() + name.size() + 1> result{};
        std::size_t i = 0;
        for (const auto byte : prefix) result[i++] = byte;
        for (const auto byte : name) result[i++] = byte;
        return result;
    }();
    return RetainedString::fromStaticView({bytes.data(), bytes.size() - 1});
}
template<class T> inline constexpr bool unicodeElement = std::same_as<std::remove_cv_t<T>, char>
    || std::same_as<std::remove_cv_t<T>, char8_t> || std::same_as<std::remove_cv_t<T>, char16_t>
    || std::same_as<std::remove_cv_t<T>, char32_t> || std::same_as<std::remove_cv_t<T>, wchar_t>;
#ifdef NG_NO_AUTODECODE_STRINGS
inline constexpr bool autoDecodeStrings = false;
#else
inline constexpr bool autoDecodeStrings = true;
#endif

template<class R> concept IndexedRange = requires { typename IndexedRangeTraits<R>::value_type; }
    && (!std::is_void_v<typename IndexedRangeTraits<R>::value_type>)
    && (!unicodeElement<typename IndexedRangeTraits<R>::value_type>
        || std::same_as<std::remove_cv_t<typename IndexedRangeTraits<R>::value_type>, char32_t>
        || (!autoDecodeStrings
            && !std::same_as<std::remove_cv_t<typename IndexedRangeTraits<R>::value_type>, wchar_t>));
template<class R> concept UnicodeRange = autoDecodeStrings
    && requires { typename IndexedRangeTraits<R>::value_type; }
    && (std::same_as<std::remove_cv_t<typename IndexedRangeTraits<R>::value_type>, char>
        || std::same_as<std::remove_cv_t<typename IndexedRangeTraits<R>::value_type>, char8_t>
        || std::same_as<std::remove_cv_t<typename IndexedRangeTraits<R>::value_type>, char16_t>);

template<class R> concept MappedSourceRange = MemberRange<R> || IndexedRange<R> || UnicodeRange<R>;
// Explicit records for this provider's own Source declarations. Arbitrary native
// fields named infinite/isInfinite are not Phobos's compile-time empty test.
template<class R> struct DeclaredInfiniteRange : std::false_type {};
template<class Cursor, class Predicate>
struct DeclaredInfiniteRange<FilterResult<Cursor, Predicate>> : std::bool_constant<Cursor::infinite> {};
template<class Cursor, class Predicate>
struct DeclaredInfiniteRange<UniqResult<Cursor, Predicate>> : std::bool_constant<Cursor::infinite> {};

template<class R> constexpr bool sourceInfinite() {
    using Declaration = SourceMemberType<R>;
    if constexpr (StaticEmpty<Declaration> && MemberRange<R>) return !Declaration::empty;
    else if constexpr (StaticEmptyMethod<Declaration> && MemberRange<R>) return !Declaration::empty();
    else return DeclaredInfiniteRange<std::remove_cv_t<Declaration>>::value;
}
template<class Expression, class = void> struct FrontExpressionValue {
    using type = std::remove_reference_t<Expression>;
};
template<class Expression> requires SourceReferenceType<std::remove_cvref_t<Expression>>::value
struct FrontExpressionValue<Expression> {
    using type = typename SourceReferenceType<std::remove_cvref_t<Expression>>::element_type;
};

template<class R> struct FrontValue {
    using type = typename FrontExpressionValue<decltype(detail::readFront(std::declval<R&>()))>::type;
};
template<class R, class = void> struct FrontDeclaration : FrontValue<R> {};
template<class R> struct FrontDeclaration<R, std::void_t<typename SourceRangeValueDeclaration<R>::type>>
    : SourceRangeValueDeclaration<R> {};
template<class Cursor, class Predicate> struct FrontDeclaration<FilterResult<Cursor, Predicate>> {
    using type = typename Cursor::source_value_declaration;
};
template<class Cursor, class Predicate> struct FrontDeclaration<UniqResult<Cursor, Predicate>> {
    using type = typename Cursor::source_value_declaration;
};

template<MemberRange R> class MemberCursor {
public:
    using value_type = typename FrontValue<R>::type;
    using source_value_declaration = typename FrontDeclaration<R>::type;
    static constexpr bool infinite = sourceInfinite<R>();
    static constexpr bool sourceDestructor = SourceValueOperations<R>::hasDestructor;
    static constexpr bool sourcePostblit = SourceValueOperations<R>::hasPostblit;
    MemberCursor() : input_(SourceValueOperations<R>::initialize()) {}
    explicit MemberCursor(R& input) : input_(SourceValueOperations<R>::copy(input)) {}
    MemberCursor(MemberCursor& other) : input_(SourceValueOperations<R>::copy(other.input_)) {}
    MemberCursor(const MemberCursor& other)
        requires requires(const R& value) { SourceValueOperations<R>::copy(value); }
        : input_(SourceValueOperations<R>::copy(other.input_)) {}
    MemberCursor(MemberCursor&& other) : input_(SourceValueOperations<R>::transport(other.input_)) {}
    MemberCursor& operator=(MemberCursor& other) {
        SourceValueOperations<R>::assign(input_, other.input_);
        return *this;
    }
    MemberCursor& operator=(const MemberCursor& other)
        requires requires(R& destination, const R& source) { SourceValueOperations<R>::assign(destination, source); } {
        SourceValueOperations<R>::assign(input_, other.input_);
        return *this;
    }
    MemberCursor& operator=(MemberCursor&& other) { return *this = other; }
    void assignFrom(MemberCursor& other) { SourceValueOperations<R>::assign(input_, other.input_); }
    void swap(MemberCursor& other) noexcept(noexcept(SourceValueOperations<R>::swap(input_, other.input_))) {
        SourceValueOperations<R>::swap(input_, other.input_);
    }
    template<class Owner> static void retireAssignedArgument(Owner& owner) {
        SourceValueOperations<R>::retireAssignedArgument(owner);
    }
    template<class Owner> static void constructorFailure(Owner& owner) {
        SourceValueOperations<R>::constructorFailure(owner);
    }
    bool empty() { return detail::readEmpty(input_); }
    decltype(auto) front() { return detail::readFront(input_); }
    decltype(auto) referenceFront() requires requires(R& value) { sourceMemberReceiver(value)->sourceReferenceFront(); } {
        return sourceMemberReceiver(input_)->sourceReferenceFront();
    }
    void popFront() { detail::popFront(input_); }
    decltype(auto) back() requires BackProperty<R> { return detail::readBack(input_); }
    decltype(auto) referenceBack() requires requires(R& value) { sourceMemberReceiver(value)->sourceReferenceBack(); } {
        return sourceMemberReceiver(input_)->sourceReferenceBack();
    }
    void popBack() requires PopBackMethod<R> { detail::popBack(input_); }
    auto save() requires SourceForwardRange<R> {
        decltype(auto) saved = detail::readSave(input_);
        if constexpr (std::is_reference_v<decltype(saved)>) return MemberCursor(saved);
        else return MemberCursor(SavedValue{}, saved);
    }
private:
    struct SavedValue {};
    MemberCursor(SavedValue, R& value) : input_(SourceValueOperations<R>::transport(value)) {}
    R input_;
};

template<IndexedRange R> class IndexedCursor {
public:
    using value_type = typename IndexedRangeTraits<R>::value_type;
    using source_value_declaration = std::conditional_t<
        std::same_as<typename IndexedRangeTraits<R>::source_element_type, SourceStringCodeUnit>,
        SourceStringCodeUnit, value_type>;
    static constexpr bool infinite = false;
    static constexpr bool sourceDestructor = SourceValueOperations<R>::hasDestructor;
    static constexpr bool sourcePostblit = SourceValueOperations<R>::hasPostblit;
    IndexedCursor() : input_(SourceValueOperations<R>::initialize()) {}
    explicit IndexedCursor(R& input) : input_(SourceValueOperations<R>::copy(input)), last_(input_.size()) {}
    IndexedCursor(IndexedCursor& other)
        : input_(SourceValueOperations<R>::copy(other.input_)), first_(other.first_), last_(other.last_) {}
    IndexedCursor(const IndexedCursor& other)
        requires requires(const R& source) { SourceValueOperations<R>::copy(source); }
        : input_(SourceValueOperations<R>::copy(other.input_)), first_(other.first_), last_(other.last_) {}
    IndexedCursor(IndexedCursor&& other)
        : input_(SourceValueOperations<R>::transport(other.input_)), first_(other.first_), last_(other.last_) {}
    bool empty() const { return first_ == last_; }
    decltype(auto) front() {
        if (empty()) {
#ifdef NDEBUG
            throw ngStaticError<ArrayIndexError>(0, 0,
                RetainedString::fromStaticLiteral("std/range/primitives.d"),2610);
#else
            throw ngStaticError<AssertError>(
                indexedArrayMessage<R, ArrayOperation::Front>(),
                RetainedString::fromStaticLiteral("std/range/primitives.d"),2609);
#endif
        }
        return element(first_);
    }
    void popFront() {
        if (empty()) {
#ifdef NDEBUG
            throw ngStaticError<ArraySliceError>(1, 0, 0,
                RetainedString::fromStaticLiteral("std/range/primitives.d"),2413);
#else
            throw ngStaticError<AssertError>(
                indexedArrayMessage<R, ArrayOperation::PopFront>(),
                RetainedString::fromStaticLiteral("std/range/primitives.d"),2412);
#endif
        }
        ++first_;
    }
    decltype(auto) back() {
        if (empty()) {
#ifdef NDEBUG
            throw ngStaticError<ArrayIndexError>(std::size_t(-1), 0,
                RetainedString::fromStaticLiteral("std/range/primitives.d"),2656);
#else
            throw ngStaticError<AssertError>(
                indexedArrayMessage<R, ArrayOperation::Back>(),
                RetainedString::fromStaticLiteral("std/range/primitives.d"),2655);
#endif
        }
        return element(last_ - 1);
    }
    void popBack() {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("std/range/primitives.d"),2527);
#endif
        // Source a[0 .. $ - 1] still checks the wrapped upper bound in release.
        if (empty()) throw ngStaticError<ArraySliceError>(0, std::size_t(-1), 0,
            RetainedString::fromStaticLiteral("std/range/primitives.d"),2528);
        --last_;
    }
    auto save() const { return *this; }
    auto referenceFront() requires SourceByteReferenceElement<value_type> {
        decltype(auto) cell = front();
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else return SourceReference<value_type>::fromRetainedObjectCell(input_.retentionOwner(), cell);
    }
    auto referenceBack() requires SourceByteReferenceElement<value_type> {
        decltype(auto) cell = back();
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else return SourceReference<value_type>::fromRetainedObjectCell(input_.retentionOwner(), cell);
    }
    void assignFrom(IndexedCursor& other) {
        SourceValueOperations<R>::assign(input_, other.input_);
        first_ = other.first_;
        last_ = other.last_;
    }
    void swap(IndexedCursor& other) noexcept(noexcept(SourceValueOperations<R>::swap(input_, other.input_))) {
        SourceValueOperations<R>::swap(input_, other.input_);
        std::swap(first_, other.first_);
        std::swap(last_, other.last_);
    }
    template<class Owner> static void retireAssignedArgument(Owner& owner) {
        SourceValueOperations<R>::retireAssignedArgument(owner);
    }
private:
    R input_;
    std::size_t first_ = 0, last_ = 0;
    decltype(auto) element(std::size_t index) {
        if constexpr (std::same_as<std::remove_const_t<value_type>, char8_t>)
            return SourceReference<value_type>::fromCodeUnitsUnchecked(input_, index);
        else if constexpr (requires { input_[index]; }) return input_[index];
        else return input_.data()[index];
    }
};
// std.range.primitives owns the range-operation assertions. The shared cursor
// remains the only Unicode decoder and the only code-unit traversal state.
template<class R, class Unit> class UnicodeRangeCursor {
public:
    using value_type = char32_t;
    using source_value_declaration = char32_t;
    static constexpr bool infinite = false;
    static constexpr bool sourceDestructor = SourceValueOperations<R>::hasDestructor;
    static constexpr bool sourcePostblit = SourceValueOperations<R>::hasPostblit;
    static_assert(!sourceDestructor && !sourcePostblit,
        "The Unicode backend requires a genuine intrinsic array descriptor");
    UnicodeRangeCursor() : input_(SourceValueOperations<R>::initialize()) {}
    explicit UnicodeRangeCursor(R& input) : input_(SourceValueOperations<R>::copy(input)) {}
    bool empty() const { return input_.empty(); }
    char32_t front() const {
        if (empty()) {
#ifndef NDEBUG
            throw ngStaticError<AssertError>(
                indexedArrayMessage<R, ArrayOperation::Front, true>(),
                RetainedString::fromStaticLiteral("std/range/primitives.d"), 2639);
#else
            throw ngStaticError<ArrayIndexError>(0, 0,
                RetainedString::fromStaticLiteral("std/utf.d"), 1183);
#endif
        }
        return input_.front();
    }
    void popFront() {
        if (empty()) {
#ifndef NDEBUG
            throw ngStaticError<AssertError>(
                indexedArrayMessage<R, ArrayOperation::PopFront, true>(),
                RetainedString::fromStaticLiteral("std/range/primitives.d"), 2437);
#else
            throw ngStaticError<ArrayIndexError>(0, 0,
                RetainedString::fromStaticLiteral("std/range/primitives.d"), sizeof(Unit) == 1 ? 2448 : 2454);
#endif
        }
        input_.popFront();
    }
    char32_t back() const {
        if (empty()) {
#ifndef NDEBUG
            throw ngStaticError<AssertError>(
                indexedArrayMessage<R, ArrayOperation::Back, true>(),
                RetainedString::fromStaticLiteral("std/range/primitives.d"), 2683);
#else
            throw ngStaticError<ArrayIndexError>(std::size_t(-1), 0,
                RetainedString::fromStaticLiteral("std/utf.d"), sizeof(Unit) == 1 ? 724 : 850);
#endif
        }
        if constexpr (sizeof(Unit) == 2) {
            try { return input_.back(); }
            catch (const std::out_of_range&) {
                // Only the backend's one-low-surrogate guard can reach here:
                // empty is handled above and these are intrinsic array reads.
#ifndef NDEBUG
                throw ngStaticError<AssertError>(
                    RetainedString::fromStaticLiteral("Attempted to decode past the end of a string"),
                    RetainedString::fromStaticLiteral("std/utf.d"), 1175);
#else
                throw ngStaticError<ArrayIndexError>(std::size_t(-1), 1,
                    RetainedString::fromStaticLiteral("std/utf.d"), 1183);
#endif
            }
        } else return input_.back();
    }
    void popBack() {
        if (empty()) {
#ifndef NDEBUG
            throw ngStaticError<AssertError>(
                indexedArrayMessage<R, ArrayOperation::PopBack, true>(),
                RetainedString::fromStaticLiteral("std/range/primitives.d"), 2551);
#else
            throw ngStaticError<ArrayIndexError>(std::size_t(-1), 0,
                RetainedString::fromStaticLiteral("std/utf.d"), sizeof(Unit) == 1 ? 724 : 850);
#endif
        }
        if constexpr (sizeof(Unit) == 2) {
            try { input_.popBack(); }
            catch (const std::out_of_range&) {
                throw ngStaticError<ArraySliceError>(0, std::size_t(-1), 1,
                    RetainedString::fromStaticLiteral("std/range/primitives.d"), 2552);
            }
        } else input_.popBack();
    }
    auto save() const { return *this; }
    void assignFrom(UnicodeRangeCursor& other) { *this = other; }
    void swap(UnicodeRangeCursor& other) noexcept(noexcept(std::swap(input_, other.input_))) {
        std::swap(input_, other.input_);
    }
private:
    UnicodeCursor<R, Unit> input_;
};

template<MappedSourceRange R> auto cursor(R& input) {
    if constexpr (MemberRange<R>) return MemberCursor<R>(input);
    else if constexpr (UnicodeRange<R>)
        return UnicodeRangeCursor<R, typename IndexedRangeTraits<R>::value_type>(input);
    else {
        return IndexedCursor<R>(input);
    }
}
template<MappedSourceRange R> using CursorFor = decltype(cursor(std::declval<R&>()));

template<class Cursor> concept BidirectionalCursor = requires(Cursor& input) {
    { input.save() } -> std::same_as<Cursor>;
    input.popBack();
    requires std::same_as<std::remove_reference_t<decltype(input.back())>, typename Cursor::value_type>;
};

// Own one actual cursor value without an optional/tuple forwarding constructor.
// Direct member initialization elides returned value temporaries. The defining
// selected-ABI retirement operation controls generated opAssign argument cleanup.
template<class Cursor> class CursorField {
    struct Move {};
    union Storage {
        Cursor value;
        Storage() : value() {}
        Storage(Cursor& source) : value(source) {}
        Storage(const Cursor& source) requires requires { Cursor(source); } : value(source) {}
        Storage(Move, Cursor& source) : value(std::move(source)) {}
        ~Storage() {}
    } storage_;
    bool active_ = true;
public:
    CursorField() = default;
    CursorField(CursorField& other) : storage_(other.storage_.value) {}
    CursorField(const CursorField& other) requires requires(const Cursor& source) { Cursor(source); }
        : storage_(other.storage_.value) {}
    CursorField(CursorField&& other) : storage_(Move{}, other.storage_.value) {}
    ~CursorField() noexcept(std::is_nothrow_destructible_v<Cursor>) { retire(); }
    void retire() noexcept(std::is_nothrow_destructible_v<Cursor>) {
        if (!active_) return;
        active_ = false;
        std::destroy_at(std::addressof(storage_.value));
    }
    void assignFrom(Cursor& source) { storage_.value.assignFrom(source); }
    void assignFrom(CursorField& source) { assignFrom(source.storage_.value); }
    void swap(CursorField& other) noexcept(noexcept(storage_.value.swap(other.storage_.value))) {
        storage_.value.swap(other.storage_.value);
    }
    bool empty() { return storage_.value.empty(); }
    decltype(auto) front() { return storage_.value.front(); }
    decltype(auto) referenceFront() requires requires(Cursor& cursor) { cursor.referenceFront(); } {
        return storage_.value.referenceFront();
    }
    void popFront() { storage_.value.popFront(); }
    decltype(auto) back() requires BidirectionalCursor<Cursor> { return storage_.value.back(); }
    decltype(auto) referenceBack() requires requires(Cursor& cursor) { cursor.referenceBack(); } {
        return storage_.value.referenceBack();
    }
    void popBack() requires BidirectionalCursor<Cursor> { storage_.value.popBack(); }
    auto save() requires requires(Cursor& input) { input.save(); } { return storage_.value.save(); }
};
template<class Cursor> struct CursorFrontExpression {
    Cursor& input;
    decltype(auto) operator()() const { return input.front(); }
    decltype(auto) reference() const requires requires { input.referenceFront(); } { return input.referenceFront(); }
};

template<class Cursor> struct CursorBackExpression {
    Cursor& input;
    decltype(auto) operator()() const { return input.back(); }
    decltype(auto) reference() const requires requires { input.referenceBack(); } { return input.referenceBack(); }
};
template<std::size_t Index, class Predicate, class Cursor>
    requires requires(Predicate& target, Cursor& input) {
        detail::bindFrontExpression<Index>(target, CursorFrontExpression<Cursor>{input});
    }
decltype(auto) boundCursorFront(Predicate& target, Cursor& input) {
    return detail::bindFrontExpression<Index>(target, CursorFrontExpression<Cursor>{input});
}

template<class Predicate> using PreparedPredicate = decltype(ngPrepareSourceCallable(std::declval<Predicate&>()));
template<class Predicate, class Range> concept UniqPredicate = MappedSourceRange<Range>
    && requires(std::remove_reference_t<PreparedPredicate<Predicate>>& target, CursorFor<Range>& input) {
        requires std::same_as<std::remove_reference_t<
            decltype(::nijigenerate::compat::ngInvokeSourceCallable(
                std::declval<PreparedPredicate<Predicate>>(),
                detail::boundCursorFront<0>(target, input), detail::boundCursorFront<1>(target, input)))>, bool>;
    };

template<class T> struct SharedClassHandle : std::false_type {};
template<class T> struct SharedClassHandle<std::shared_ptr<T>> : std::bool_constant<std::is_class_v<T>> {};

template<class T> struct EqualityArrayCarrier : std::false_type {};
template<class T> struct EqualityArrayCarrier<Slice<T>> : std::true_type {};
template<class T> struct EqualityArrayCarrier<ConstSlice<T>> : std::true_type {};
template<class T> struct EqualityArrayCarrier<nicxlive::core::common::SharedArray<T>> : std::true_type {};

template<class T, std::size_t N> struct EqualityArrayCarrier<std::array<T, N>> : std::true_type {};

template<class T> struct EqualityStaticArrayCarrier : std::false_type {};

template<class T, std::size_t N> struct EqualityStaticArrayCarrier<std::array<T, N>> : std::true_type {};

template<class Array> decltype(auto) equalityArrayOperand(Array&& value) {
    if constexpr (EqualityStaticArrayCarrier<std::remove_cvref_t<Array>>::value)
        return std::forward<Array>(value);
    else return std::remove_cvref_t<Array>(std::forward<Array>(value));
}

template<class T> concept DeclaredSourceValue = requires {
    requires (!::nicxlive::core::d_runtime::SourceTypeSpellings<T>::isNativeObject);
};

template<class T> inline constexpr bool valueEqualityReceiver =
    !::nicxlive::core::d_runtime::detail::sourceNativeObjectOrigin<T>
    && (!std::is_polymorphic_v<T> || DeclaredSourceValue<T>);

template<class T, class = void> struct SingleValueEqualsMethod : std::false_type {};
template<class T> struct SingleValueEqualsMethod<T, std::void_t<decltype(&T::opEquals)>>
    : std::bool_constant<valueEqualityReceiver<T>
        && std::is_member_function_pointer_v<decltype(&T::opEquals)>> {};

template<class A, class B> concept ValueEqualsCall = requires(A&& a, B&& b) {
    std::forward<A>(a).opEquals(std::forward<B>(b));
    requires (!std::is_void_v<decltype(std::forward<A>(a).opEquals(std::forward<B>(b)))>);
};

template<class A, class B> concept NativeEqualsCall = requires(A&& a, B&& b) {
    std::forward<A>(a) == std::forward<B>(b);
    requires (!std::is_void_v<decltype(std::forward<A>(a) == std::forward<B>(b))>);
};

template<class T> constexpr auto sourceArithmeticValue(T value) noexcept {
    // D char is an unsigned UTF-8 code unit even when native char is signed.
    // Keep byte/sbyte and all other arithmetic declarations unchanged.
    if constexpr (std::same_as<std::remove_cv_t<T>, char>) return static_cast<unsigned char>(value);
    else return value;
}

template<class A, class B> concept UnambiguousValueEquals =
    std::same_as<std::remove_cvref_t<A>, std::remove_cvref_t<B>>
    && !SharedClassHandle<std::remove_cvref_t<A>>::value
    && valueEqualityReceiver<std::remove_cvref_t<A>>
    && (SingleValueEqualsMethod<std::remove_cvref_t<A>>::value || std::same_as<A, B>)
    && (ValueEqualsCall<A, B> || ValueEqualsCall<B, A>)
    && !NativeEqualsCall<A, B>;

struct Equal {
    template<class A, class B> requires std::derived_from<A, nicxlive::core::Resource>
        && std::derived_from<B, nicxlive::core::Resource>
    bool operator()(const std::shared_ptr<A>& a, const std::shared_ptr<B>& b) const {
        return nicxlive::core::ngResourceObjectEquals(a, b);
    }
    template<class T> requires (!std::derived_from<T, nicxlive::core::Resource>)
        && ::nicxlive::core::d_runtime::detail::sourceNativeObjectOrigin<T>
        && std::is_polymorphic_v<T>
        && requires(T& object, std::shared_ptr<T> other) {
            { object.opEquals(other) } -> std::same_as<bool>;
        }
    bool operator()(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b) const {
        return nicxlive::core::common::ngClassObjectEquals(a, b);
    }
    template<class A, class B> requires UnambiguousValueEquals<A, B>
    decltype(auto) operator()(A&& a, B&& b) const {
        if constexpr (ValueEqualsCall<A, B>)
            return std::forward<A>(a).opEquals(std::forward<B>(b));
        else
            return std::forward<B>(b).opEquals(std::forward<A>(a));
    }
    template<class A, class B>
        requires (!SharedClassHandle<std::remove_cvref_t<A>>::value
            && !SharedClassHandle<std::remove_cvref_t<B>>::value
            && !EqualityArrayCarrier<std::remove_cvref_t<A>>::value
            && !EqualityArrayCarrier<std::remove_cvref_t<B>>::value)
        && NativeEqualsCall<A, B>
    decltype(auto) operator()(A&& a, B&& b) const {
        if constexpr (std::is_arithmetic_v<std::remove_cvref_t<A>>
            && std::is_arithmetic_v<std::remove_cvref_t<B>>)
            return sourceArithmeticValue(a) == sourceArithmeticValue(b);
        else return std::forward<A>(a) == std::forward<B>(b);
    }
    // The actual canonical runtime vector structs have no scalar operator==.
    // Map their real inmath Vector.opEquals component-array semantics explicitly.
    bool operator()(nicxlive::core::math::Vec2 a, nicxlive::core::math::Vec2 b) const {
        return a.x == b.x && a.y == b.y;
    }
    bool operator()(nicxlive::core::math::Vec3 a, nicxlive::core::math::Vec3 b) const {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool operator()(nicxlive::core::math::Vec4 a, nicxlive::core::math::Vec4 b) const {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }
    template<class A, class B>
        requires (EqualityArrayCarrier<std::remove_cvref_t<A>>::value
            && EqualityArrayCarrier<std::remove_cvref_t<B>>::value)
        && requires(const Equal& compare, A&& a, B&& b) {
            { ngSourceBool(compare(*a.data(), *b.data())) } -> std::same_as<bool>;
        }
    bool operator()(A&& a, B&& b) const {
        decltype(auto) left = equalityArrayOperand(std::forward<A>(a));
        decltype(auto) right = equalityArrayOperand(std::forward<B>(b));
        const auto length = left.size();
        if (length != right.size()) return false;
        // Dynamic descriptors retain backing; static arrays keep their real cells.
        // There is no identity shortcut: an array containing NaN differs from itself.
        for (std::size_t i = 0; i < length; ++i)
            if (!ngSourceBool((*this)(left.data()[i], right.data()[i]))) return false;
        return true;
    }
};
}

template<> struct SourceValueOperations<detail::Equal> : detail::DescriptorValueOperations<detail::Equal> {};
template<> struct SourcePredicateBinding<detail::Equal> : detail::DescriptorValueOperations<detail::Equal> {};

template<class Cursor, class Predicate> class FilterResult {
public:
    using value_type = typename Cursor::value_type;
    using source_value_declaration = typename Cursor::source_value_declaration;
    static constexpr bool infinite = Cursor::infinite;
    static constexpr bool sourceDestructor = Cursor::sourceDestructor;
    static constexpr bool sourcePostblit = Cursor::sourcePostblit;
    FilterResult() : predicate_(SourcePredicateBinding<Predicate>::initialize()) {}
    template<class BoundPredicate> FilterResult(Cursor&& input, BoundPredicate& predicate) : FilterResult() {
        try {
            input_.assignFrom(input);
            SourcePredicateBinding<Predicate>::assign(predicate_, predicate);
        } catch (...) {
            if constexpr (sourceDestructor) Cursor::constructorFailure(input_);
            throw;
        }
    }
    FilterResult(FilterResult& other)
        : input_(other.input_), predicate_(SourcePredicateBinding<Predicate>::copy(other.predicate_)),
          primed_(other.primed_) {}
    FilterResult(const FilterResult& other)
        requires requires(const Predicate& predicate, const Cursor& input) {
            SourcePredicateBinding<Predicate>::copy(predicate);
            Cursor(input);
        }
        : input_(other.input_), predicate_(SourcePredicateBinding<Predicate>::copy(other.predicate_)),
          primed_(other.primed_) {}
    FilterResult(FilterResult&& other)
        : input_(std::move(other.input_)), predicate_(SourcePredicateBinding<Predicate>::transport(other.predicate_)),
          primed_(other.primed_) {}
    FilterResult& operator=(FilterResult other) {
        // Selected DMD clone.d buildOpAssign: destructor/postblit structs blit
        // the copied parameter; otherwise assignment is memberwise and invokes
        // the actual nested range's opAssign. These are distinct Source bodies.
        if constexpr (Cursor::sourceDestructor || Cursor::sourcePostblit) {
            swap(other);
            if constexpr (Cursor::sourceDestructor) Cursor::retireAssignedArgument(other.input_);
        } else {
            input_.assignFrom(other.input_);
            SourcePredicateBinding<Predicate>::assign(predicate_, other.predicate_);
            primed_ = other.primed_;
        }
        return *this;
    }
    void swap(FilterResult& other) noexcept(noexcept(input_.swap(other.input_))
        && noexcept(SourcePredicateBinding<Predicate>::swap(predicate_, other.predicate_))) {
        static_assert(noexcept(input_.swap(other.input_)), "Source value swap must be passive nonthrowing transport");
        static_assert(noexcept(SourcePredicateBinding<Predicate>::swap(predicate_, other.predicate_)),
            "Source predicate alias swap must be passive nonthrowing transport");
        input_.swap(other.input_);
        SourcePredicateBinding<Predicate>::swap(predicate_, other.predicate_);
        std::swap(primed_, other.primed_);
    }
    bool empty() {
        if constexpr (infinite) return false;
        else { prime(); return input_.empty(); }
    }
    decltype(auto) front() {
        prime();
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Attempting to fetch the front of an empty filter."),
            RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 1384);
#endif
        return input_.front();
    }
    decltype(auto) sourceReferenceFront() requires requires(detail::CursorField<Cursor>& input) {
        input.referenceFront();
    } {
        prime();
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Attempting to fetch the front of an empty filter."),
            RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 1384);
#endif
        return input_.referenceFront();
    }
    void popFront() {
        prime();
        do { input_.popFront(); }
        while (!input_.empty() && !matchesFront());
    }
    auto save() requires requires(Cursor& r) { r.save(); } {
        auto saved = input_.save();
        const bool primed = primed_;
        return FilterResult(std::move(saved), predicate_, primed);
    }
    auto slice() { return *this; }
    template<class Owner> static void retireAssignedArgument(Owner& owner) {
        Cursor::retireAssignedArgument(owner);
    }
    template<class Owner> static void constructorFailure(Owner& owner) {
        Cursor::constructorFailure(owner);
    }
private:
    template<class BoundPredicate>
    FilterResult(Cursor&& input, BoundPredicate& predicate, bool primed) : FilterResult(std::move(input), predicate) {
        primed_ = primed;
    }
    void prime() {
        if (primed_) return;
        while (!input_.empty() && !matchesFront()) input_.popFront();
        primed_ = true;
    }
    bool matchesFront() {
        decltype(auto) target = ::nijigenerate::compat::ngPrepareSourceCallable(predicate_);
        detail::CursorFrontExpression read{input_};
        SourceCallArguments<decltype(detail::bindFrontExpression<0>(target, read))> arguments{
            {detail::bindFrontExpression<0>(target, read)}};
        return ngSourceBool(::nijigenerate::compat::ngInvokeSourceCallable(
            std::forward<decltype(target)>(target), ngSourceCallArgument<0>(arguments)));
    }
    detail::CursorField<Cursor> input_;
    Predicate predicate_;
    bool primed_ = false;
};

template<class Cursor, class Predicate> class UniqResult {
public:
    using value_type = typename Cursor::value_type;
    using source_value_declaration = typename Cursor::source_value_declaration;
    static constexpr bool infinite = Cursor::infinite;
    static constexpr bool sourceDestructor = Cursor::sourceDestructor;
    static constexpr bool sourcePostblit = Cursor::sourcePostblit;
    UniqResult() : predicate_(SourcePredicateBinding<Predicate>::initialize()) {}
    template<class BoundPredicate> UniqResult(Cursor&& input, BoundPredicate& predicate) : UniqResult() {
        try {
            input_.assignFrom(input);
            SourcePredicateBinding<Predicate>::assign(predicate_, predicate);
        } catch (...) {
            if constexpr (sourceDestructor) Cursor::constructorFailure(input_);
            throw;
        }
    }
    UniqResult(UniqResult& other)
        : input_(other.input_), predicate_(SourcePredicateBinding<Predicate>::copy(other.predicate_)) {}
    UniqResult(const UniqResult& other)
        requires requires(const Predicate& predicate, const Cursor& input) {
            SourcePredicateBinding<Predicate>::copy(predicate);
            Cursor(input);
        }
        : input_(other.input_), predicate_(SourcePredicateBinding<Predicate>::copy(other.predicate_)) {}
    UniqResult(UniqResult&& other)
        : input_(std::move(other.input_)), predicate_(SourcePredicateBinding<Predicate>::transport(other.predicate_)) {}
    UniqResult& operator=(UniqResult other) {
        if constexpr (Cursor::sourceDestructor || Cursor::sourcePostblit) {
            swap(other);
            if constexpr (Cursor::sourceDestructor) Cursor::retireAssignedArgument(other.input_);
        } else {
            input_.assignFrom(other.input_);
            SourcePredicateBinding<Predicate>::assign(predicate_, other.predicate_);
        }
        return *this;
    }
    void swap(UniqResult& other) noexcept(noexcept(input_.swap(other.input_))
        && noexcept(SourcePredicateBinding<Predicate>::swap(predicate_, other.predicate_))) {
        static_assert(noexcept(input_.swap(other.input_)), "Source value swap must be passive nonthrowing transport");
        static_assert(noexcept(SourcePredicateBinding<Predicate>::swap(predicate_, other.predicate_)),
            "Source predicate alias swap must be passive nonthrowing transport");
        input_.swap(other.input_);
        SourcePredicateBinding<Predicate>::swap(predicate_, other.predicate_);
    }
    bool empty() {
        if constexpr (infinite) return false;
        else return input_.empty();
    }
    value_type front() {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Attempting to fetch the front of an empty uniq."),
            RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 7853);
#endif
        if constexpr (std::is_reference_v<decltype(input_.front())>)
            return SourceValueOperations<std::remove_cv_t<value_type>>::copy(input_.front());
        else return input_.front();
    }
    void popFront() {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Attempting to popFront an empty uniq."),
            RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 7842);
#endif
        std::remove_cv_t<value_type> last = [&]() -> std::remove_cv_t<value_type> {
            if constexpr (std::is_reference_v<decltype(input_.front())>)
                return SourceValueOperations<std::remove_cv_t<value_type>>::copy(input_.front());
            else return input_.front();
        }();
        do { input_.popFront(); }
        while (!input_.empty() && matchesFront(last));
    }
    value_type back() requires detail::BidirectionalCursor<Cursor> {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Attempting to fetch the back of an empty uniq."),
            RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 7872);
#endif
        if constexpr (std::is_reference_v<decltype(input_.back())>)
            return SourceValueOperations<std::remove_cv_t<value_type>>::copy(input_.back());
        else return input_.back();
    }
    void popBack() requires detail::BidirectionalCursor<Cursor> {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Attempting to popBack an empty uniq."),
            RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 7861);
#endif
        std::remove_cv_t<value_type> last = [&]() -> std::remove_cv_t<value_type> {
            if constexpr (std::is_reference_v<decltype(input_.back())>)
                return SourceValueOperations<std::remove_cv_t<value_type>>::copy(input_.back());
            else return input_.back();
        }();
        do { input_.popBack(); }
        while (!input_.empty() && matchesBack(last));
    }
    auto save() requires requires(Cursor& r) { r.save(); } { return UniqResult(input_.save(), predicate_); }
    auto slice() { return *this; }
    template<class Owner> static void retireAssignedArgument(Owner& owner) {
        Cursor::retireAssignedArgument(owner);
    }
    template<class Owner> static void constructorFailure(Owner& owner) {
        Cursor::constructorFailure(owner);
    }
private:
    bool matchesFront(std::remove_cv_t<value_type>& last) {
        decltype(auto) target = ::nijigenerate::compat::ngPrepareSourceCallable(predicate_);
        auto left = [&]() -> decltype(auto) { return (last); };
        detail::CursorFrontExpression right{input_};
        SourceCallArguments<decltype(detail::bindFrontExpression<0>(target, left)),
            decltype(detail::bindFrontExpression<1>(target, right))> arguments{
            {detail::bindFrontExpression<0>(target, left)}, {detail::bindFrontExpression<1>(target, right)}};
        return ::nijigenerate::compat::ngInvokeSourceCallable(
            std::forward<decltype(target)>(target),
            ngSourceCallArgument<0>(arguments), ngSourceCallArgument<1>(arguments));
    }
    bool matchesBack(std::remove_cv_t<value_type>& last) requires detail::BidirectionalCursor<Cursor> {
        decltype(auto) target = ::nijigenerate::compat::ngPrepareSourceCallable(predicate_);
        auto left = [&]() -> decltype(auto) { return (last); };
        detail::CursorBackExpression right{input_};
        SourceCallArguments<decltype(detail::bindFrontExpression<0>(target, left)),
            decltype(detail::bindFrontExpression<1>(target, right))> arguments{
            {detail::bindFrontExpression<0>(target, left)}, {detail::bindFrontExpression<1>(target, right)}};
        return ::nijigenerate::compat::ngInvokeSourceCallable(
            std::forward<decltype(target)>(target),
            ngSourceCallArgument<0>(arguments), ngSourceCallArgument<1>(arguments));
    }
    detail::CursorField<Cursor> input_;
    Predicate predicate_;
};

namespace detail {
template<class T> struct ResultValueOperations {
    inline static constexpr bool hasDestructor = T::sourceDestructor;
    inline static constexpr bool hasPostblit = T::sourcePostblit;
    static T initialize() { return {}; }
    static T copy(T& source) { return T(source); }
    static T copy(const T& source) requires requires { T(source); } { return T(source); }
    static T transport(T& source) { return T(std::move(source)); }
    static void assign(T& destination, T& source) { destination = source; }
    static void swap(T& left, T& right) noexcept(noexcept(left.swap(right))) { left.swap(right); }
    template<class Owner> static void retireAssignedArgument(Owner& owner) { T::retireAssignedArgument(owner); }
    template<class Owner> static void constructorFailure(Owner& owner) {
        T::constructorFailure(owner);
    }
};
}
template<class Cursor, class Predicate> struct SourceValueOperations<FilterResult<Cursor, Predicate>>
    : detail::ResultValueOperations<FilterResult<Cursor, Predicate>> {};
template<class Cursor, class Predicate> struct SourceValueOperations<UniqResult<Cursor, Predicate>>
    : detail::ResultValueOperations<UniqResult<Cursor, Predicate>> {};

template<class Predicate, class Range> requires detail::MappedSourceRange<std::remove_cvref_t<Range>>
auto ngFilter(Predicate&& predicate, Range&& source) {
    using R = std::remove_cvref_t<Range>;
    using P = std::remove_cvref_t<Predicate>;
    auto range = [&]() -> R {
        if constexpr (std::is_lvalue_reference_v<Range>) return SourceValueOperations<R>::copy(source);
        else return SourceValueOperations<R>::transport(source);
    }();
    auto input = detail::cursor(range);
    return FilterResult<decltype(input), P>(std::move(input), predicate);
}
template<class Predicate, class Range>
    requires detail::UniqPredicate<std::remove_cvref_t<Predicate>, std::remove_cvref_t<Range>>
auto ngUniq(Predicate&& predicate, Range&& source) {
    using R = std::remove_cvref_t<Range>;
    using P = std::remove_cvref_t<Predicate>;
    auto range = [&]() -> R {
        if constexpr (std::is_lvalue_reference_v<Range>) return SourceValueOperations<R>::copy(source);
        else return SourceValueOperations<R>::transport(source);
    }();
    auto input = detail::cursor(range);
    return UniqResult<decltype(input), P>(std::move(input), predicate);
}
template<auto Predicate> struct Filter {
    template<class Range> auto operator()(Range&& range) const {
        PhobosPredicateAlias<Predicate> alias;
        return ngFilter(alias, std::forward<Range>(range));
    }
    template<class P, class Range> auto with(P&& predicate, Range&& range) const {
        return ngFilter(std::forward<P>(predicate), std::forward<Range>(range));
    }
};
template<auto Predicate = detail::Equal{}> struct Uniq {
    template<class Range>
        requires detail::UniqPredicate<PhobosPredicateAlias<Predicate>, std::remove_cvref_t<Range>>
    auto operator()(Range&& range) const {
        PhobosPredicateAlias<Predicate> alias;
        return ngUniq(alias, std::forward<Range>(range));
    }
    template<class P, class Range>
        requires detail::UniqPredicate<std::remove_cvref_t<P>, std::remove_cvref_t<Range>>
    auto with(P&& predicate, Range&& range) const {
        return ngUniq(std::forward<P>(predicate), std::forward<Range>(range));
    }
};

namespace detail {
template<class Range> struct ArrayElement : FrontValue<Range> {};
template<class Range> struct ArrayElementDeclaration : FrontDeclaration<Range> {};
// C++ scalar prvalues discard top-level const; these actual source wrappers
// already retain the original ElementType and must not lose it at .array.
template<class Cursor, class Predicate> struct ArrayElement<FilterResult<Cursor, Predicate>> {
    using type = typename Cursor::value_type;
};
template<class Cursor, class Predicate> struct ArrayElement<UniqResult<Cursor, Predicate>> {
    using type = typename Cursor::value_type;
};
}

// Concrete source .array consumer bridge. Unicode materialization and exact
// std.array allocator behavior retain their own provider gates.
template<class SourceRange> requires detail::MemberRange<std::remove_cvref_t<SourceRange>>
    && (!detail::sourceInfinite<std::remove_cvref_t<SourceRange>>())
auto ngArray(SourceRange&& source) {
    using Range = std::remove_cvref_t<SourceRange>;
    auto range = [&]() -> Range {
        if constexpr (std::is_lvalue_reference_v<SourceRange>) return SourceValueOperations<Range>::copy(source);
        else return SourceValueOperations<Range>::transport(source);
    }();
    using Value = typename detail::ArrayElement<Range>::type;
    using MutableValue = std::remove_cv_t<Value>;
    // Canonical Slice<bool> has genuine addressable bool cells, as other slices do.
    Slice<MutableValue> result;
    for (Range input = SourceValueOperations<Range>::copy(range);
        !detail::readEmpty(input); detail::popFront(input)) {
        Value item = [&]() -> Value {
            if constexpr (std::is_reference_v<decltype(detail::readFront(input))>)
                return SourceValueOperations<MutableValue>::copy(detail::readFront(input));
            else return detail::readFront(input);
        }();
        result.append(item);
    }
    if constexpr (std::same_as<typename detail::ArrayElementDeclaration<Range>::type, SourceStringCodeUnit>) {
        if (result.empty()) return RetainedString{};
        return RetainedString::retainedView(result.retentionOwner(),
            reinterpret_cast<const char*>(result.data()), result.size());
    } else if constexpr (std::is_const_v<Value>) return ConstSlice<MutableValue>(std::move(result));
    else return result;
}
} // namespace nijigenerate::compat::phobos
