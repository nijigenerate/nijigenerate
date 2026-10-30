#pragma once
#include "mir_rc_value_lifetime.hpp"
#include "mir_rc_integer_vector_lifetime.hpp"
#include <type_traits>

namespace nijigenerate::compat {
// These actual Source declarations contain only basic scalar fields/arrays.
// No native triviality trait is used to infer mutable indirections in other T.
template<class T> concept MirRCPlainValue = !std::is_const_v<T> && !std::is_volatile_v<T> &&
    ((std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) ||
     std::is_same_v<T, nicxlive::core::math::Vec2> ||
     std::is_same_v<T, nicxlive::core::math::Vec3> ||
     std::is_same_v<T, nicxlive::core::math::Vec4> ||
     std::is_same_v<T, nicxlive::core::math::Vec2i> ||
     std::is_same_v<T, nicxlive::core::math::Vec4i> ||
     requires { requires MirRCElementDefinition<T>::noMutableIndirections; });

template<class T> struct MirRCConstProjection<T, std::enable_if_t<
    MirRCPlainValue<std::remove_cv_t<T>> && !std::is_arithmetic_v<T>>>
    : MirRCPlainConstProjection<T> {};

// Shared method selection must also agree with the declared unqualified value
// bodies. A scalar-only layout alone does not prove custom shared opAssign.
template<class T> concept MirRCPlainSharedValue = MirRCPlainValue<T> &&
    ((std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) ||
     std::is_same_v<T, nicxlive::core::math::Vec2> ||
     std::is_same_v<T, nicxlive::core::math::Vec3> ||
     std::is_same_v<T, nicxlive::core::math::Vec4> ||
     std::is_same_v<T, nicxlive::core::math::Vec2i> ||
     std::is_same_v<T, nicxlive::core::math::Vec4i> ||
     requires { requires MirRCElementDefinition<T>::sharedValueOperationsEquivalent; });

// The bounded readonly profile has no Source qualifier-specific assignment or
// method behavior. Other types bind their actual qualified descriptors instead.
template<class T> concept MirRCPlainReadOnlyValue = MirRCPlainValue<T> &&
    ((std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) ||
     std::is_same_v<T, nicxlive::core::math::Vec2> ||
     std::is_same_v<T, nicxlive::core::math::Vec3> ||
     std::is_same_v<T, nicxlive::core::math::Vec4> ||
     std::is_same_v<T, nicxlive::core::math::Vec2i> ||
     std::is_same_v<T, nicxlive::core::math::Vec4i> ||
     requires { requires MirRCElementDefinition<T>::readOnlyValueOperationsEquivalent; });
} // namespace nijigenerate::compat
