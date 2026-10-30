#pragma once

#include <cstddef>
#include <utility>

namespace nijigenerate::compat {
namespace detail {

// Index distinguishes repeated types. Each expression initializes the member
// directly; a Cell-returning factory would lose that guarantee for base objects.
template<std::size_t Index, class T> struct SourceCallArgumentCell {
    T value;
};

template<class Indices, class... T> struct SourceCallArgumentsStorage;

// Aggregate elements are these public bases in declaration order. Their value
// members are destroyed in reverse order, including partial initialization.
template<std::size_t... Index, class... T>
struct SourceCallArgumentsStorage<std::index_sequence<Index...>, T...>
    : SourceCallArgumentCell<Index, T>... {};

template<std::size_t Index, class Head, class... Tail>
struct SourceCallArgumentType : SourceCallArgumentType<Index - 1, Tail...> {};

template<class Head, class... Tail> struct SourceCallArgumentType<0, Head, Tail...> {
    using type = Head;
};

} // namespace detail

// Initialize with one nested brace per expression:
// SourceCallArguments<decltype(front0()), decltype(front1())> args{
//     {front0()}, {front1()}};
// Braced clauses evaluate left to right. A value front initializes its actual T
// member with guaranteed prvalue elision; T& and T&& bind their actual referents.
// Do not wrap clauses in typed Cell temporaries or add a forwarding constructor.
template<class... T>
using SourceCallArguments = detail::SourceCallArgumentsStorage<std::index_sequence_for<T...>, T...>;

template<std::size_t Index, class... T> requires (Index < sizeof...(T))
constexpr decltype(auto) ngSourceCallArgument(SourceCallArguments<T...>& arguments) noexcept {
    using Element = typename detail::SourceCallArgumentType<Index, T...>::type;
    auto& cell = static_cast<detail::SourceCallArgumentCell<Index, Element>&>(arguments);
    // Values and declared rvalue references are forwarded as rvalues. A declared
    // lvalue reference collapses back to that exact lvalue reference type.
    return static_cast<Element&&>(cell.value);
}

} // namespace nijigenerate::compat
