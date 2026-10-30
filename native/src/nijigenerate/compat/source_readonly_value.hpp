#pragma once

#include <concepts>

namespace nijigenerate::compat {

// A defining value declaration supplies this only when const T& is its actual
// Source readonly value view, including the qualifiers of reachable fields.
// An arbitrary native const struct is not evidence of that representation.
template<class T> struct SourceReadonlyValueBinding;

template<class T> concept SourceReadonlyValueElement = requires (const T& value) {
    typename SourceReadonlyValueBinding<T>::Value;
    requires std::same_as<typename SourceReadonlyValueBinding<T>::Value, T>;
    { SourceReadonlyValueBinding<T>::read(value) } -> std::same_as<const T&>;
};

} // namespace nijigenerate::compat
