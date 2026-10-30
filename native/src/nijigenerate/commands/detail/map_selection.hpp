#pragma once

#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/commands/source_diagnostic_text.hpp>
#include <concepts>
#include <string_view>
#include <type_traits>

namespace nijigenerate::commands::detail {

template<bool Value> struct BoolString {
    inline static constexpr std::string_view value = Value ? "1" : "0";
};

template<class...> inline constexpr bool dependentMapError = false;

template<class Map> struct AssociativeMapKey {};

template<class Key, class Value, class Hash, class Equal>
struct AssociativeMapKey<core::selector::SelectorMap<Key, Value, Hash, Equal>> {
    using type = Key;
};

template<class Key, class Value, class Hash, class Equal>
struct AssociativeMapKey<CommandMapStorage<Key, Value, Hash, Equal>> {
    using type = Key;
};

template<class Map, auto AliasOrigin, bool Associative = requires {
    typename AssociativeMapKey<std::remove_cvref_t<Map>>::type;
}> struct SourceAssociativeMapKey;

template<class Map, auto AliasOrigin> struct SourceAssociativeMapKey<Map, AliasOrigin, true>
    : AssociativeMapKey<std::remove_cvref_t<Map>> {};

template<class Map, auto AliasOrigin> struct SourceAssociativeMapKey<Map, AliasOrigin, false> {
    static_assert(ngRequireSourceAssociativeArray<AliasOrigin>(),
        "Missing defining Source alias diagnostic origin for non-associative array");
};

template<class Map, auto AliasOrigin = nullptr>
using KeyTypeOfAA = typename SourceAssociativeMapKey<Map, AliasOrigin>::type;

// A binding names existing storage, never owns a map: map_type is the actual
// canonical AA type and get() returns that map in the calling thread. A TLS
// object address is deliberately not used as a non-type template argument.
template<class Binding>
concept ExistingMapBinding = requires {
    typename Binding::map_type;
    { Binding::get() } -> std::same_as<typename Binding::map_type&>;
};

template<class Binding> consteval auto commandMapAliasOrigin() {
    if constexpr (requires { Binding::sourceAliasOrigin; }) return Binding::sourceAliasOrigin;
    else return nullptr;
}

template<class Binding>
using KeyTypeOfCommandMapBinding = KeyTypeOfAA<typename Binding::map_type, commandMapAliasOrigin<Binding>()>;

template<class Key, auto KeyOrigin, class... Bindings> struct SelectMapBindingWithOrigin;

template<class Key, auto KeyOrigin> struct SelectMapBindingWithOrigin<Key, KeyOrigin> {
    static_assert(ngRequireSourceCommandsMap<Key, KeyOrigin>(),
        "Missing defining Source key diagnostic origin for exhausted command maps");
};

template<bool Matches, class Key, auto KeyOrigin, class First, class... Rest>
struct SelectMapBindingStep;

template<class Key, auto KeyOrigin, class First, class... Rest>
struct SelectMapBindingStep<true, Key, KeyOrigin, First, Rest...> {
    static_assert(ExistingMapBinding<First>, "Selected command map must name existing map storage");
    using type = First;
};

template<class Key, auto KeyOrigin, class First, class... Rest>
struct SelectMapBindingStep<false, Key, KeyOrigin, First, Rest...>
    : SelectMapBindingWithOrigin<Key, KeyOrigin, Rest...> {};

template<class Key, auto KeyOrigin, class First, class... Rest>
struct SelectMapBindingWithOrigin<Key, KeyOrigin, First, Rest...>
    : SelectMapBindingStep<std::is_same_v<Key, KeyTypeOfCommandMapBinding<First>>,
        Key, KeyOrigin, First, Rest...> {};

template<class Key, class... Bindings>
struct SelectMapBinding : SelectMapBindingWithOrigin<Key, nullptr, Bindings...> {};

template<class Key, auto KeyOrigin, class... Bindings>
using MapByKeyTypeWithOrigin = typename SelectMapBindingWithOrigin<Key, KeyOrigin, Bindings...>::type;

template<class Key, class... Bindings>
using MapByKeyType = typename SelectMapBinding<Key, Bindings...>::type;

// Resolve the selected alias at use time; never cache an address from another
// thread, clone a descriptor, initialize a registry or inspect a map entry.
template<class Key, class... Bindings>
decltype(auto) mapByKeyType() noexcept(noexcept(MapByKeyType<Key, Bindings...>::get())) {
    return MapByKeyType<Key, Bindings...>::get();
}

} // namespace nijigenerate::commands::detail
