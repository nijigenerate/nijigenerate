#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <common/shared_array.hpp>
#include <common/resource_key_map.hpp>
#include <nodes/node.hpp>
#include <param/parameter.hpp>
#include <param/binding.hpp>

#include <memory>
#include <type_traits>

namespace nijigenerate::windows::command_browser::detail {

template<class AA> struct BrowserAssociativeTypes {
    using Key = void;
    using Value = void;
};

template<class K, class V, class Hash, class Equal>
struct BrowserAssociativeTypes<core::selector::SelectorMap<K, V, Hash, Equal>> {
    using Key = K;
    using Value = V;
};
template<class K, class V, class Hash, class Equal>
struct BrowserAssociativeTypes<nicxlive::core::common::ResourceKeyMap<K, V, Hash, Equal>> {
    using Key = K;
    using Value = V;
};

template<class AA> using BrowserKeyTypeOfAA = typename BrowserAssociativeTypes<AA>::Key;
template<class AA> using BrowserValueTypeOfAA = typename BrowserAssociativeTypes<AA>::Value;

// Only the editor/runtime's actual D dynamic-array representations match.
// Do not infer array identity from iteration, element_type or a conversion.
template<class T> struct ResourceArrayType {
    static constexpr bool isDynamicArray = false;
    using Element = T;
};

template<class E> struct ResourceArrayType<compat::Slice<E>> {
    static constexpr bool isDynamicArray = true;
    using Element = E;
};

template<class E> struct ResourceArrayType<::nicxlive::core::common::SharedArray<E>> {
    static constexpr bool isDynamicArray = true;
    using Element = E;
};

template<class T> struct NodeResourceType : std::false_type {};

// Restrict conversion to genuine class handles; unrelated host conversions
// (unique_ptr, user conversion operators, raw pointers) are not D class types.
template<class N> struct NodeResourceType<std::shared_ptr<N>>
    : std::bool_constant<std::is_convertible_v<
          std::shared_ptr<N>, std::shared_ptr<::nicxlive::core::nodes::Node>>> {};

template<class T> inline constexpr bool isNodeResource = NodeResourceType<T>::value;

template<class T> inline constexpr bool isNodeResourceArray =
    ResourceArrayType<T>::isDynamicArray &&
    isNodeResource<typename ResourceArrayType<T>::Element>;

// Extraction is independent of Node eligibility, including scalar arrays.
template<class T> using NodeResourceElement = typename ResourceArrayType<T>::Element;

template<class T> inline constexpr bool isExactParameterResource =
    std::is_same_v<T, std::shared_ptr<::nicxlive::core::param::Parameter>>;

template<class T> inline constexpr bool isExactBindingResource =
    std::is_same_v<T, std::shared_ptr<::nicxlive::core::param::ParameterBinding>>;

template<class T> inline constexpr bool isResourceArgument =
    isNodeResource<T> || isNodeResourceArray<T> ||
    isExactParameterResource<T> ||
    (ResourceArrayType<T>::isDynamicArray &&
        isExactParameterResource<typename ResourceArrayType<T>::Element>) ||
    isExactBindingResource<T> ||
    (ResourceArrayType<T>::isDynamicArray &&
        isExactBindingResource<typename ResourceArrayType<T>::Element>);

} // namespace nijigenerate::windows::command_browser::detail
