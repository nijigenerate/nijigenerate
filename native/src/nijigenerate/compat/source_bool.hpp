#pragma once

#include "slice.hpp"
#include "source_const_slice_identity.hpp"
#include "retained_string.hpp"
#include "retained_string_slice.hpp"
#include "source_byte_view_identity.hpp"
#include <common/shared_array.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <concepts>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {
namespace detail {
template<class T> struct SourceReferenceTruth : std::false_type {};
template<class T> struct SourceReferenceTruth<Slice<T>> : std::true_type {};
template<class T> struct SourceReferenceTruth<ConstSlice<T>> : std::true_type {};
template<class T> struct SourceReferenceTruth<ByteView<T>> : std::true_type {};
template<class T> struct SourceReferenceTruth<nicxlive::core::common::SharedArray<T>> : std::true_type {};
template<> struct SourceReferenceTruth<RetainedString> : std::true_type {};
template<> struct SourceReferenceTruth<RetainedStringSlice> : std::true_type {};
template<class K, class V, class Hash, class Equal>
struct SourceReferenceTruth<nicxlive::core::common::ResourceKeyMap<K, V, Hash, Equal>> : std::true_type {};

template<class T> concept SourceDescriptorTruth = SourceReferenceTruth<std::remove_cvref_t<T>>::value
    && requires(T&& value) {
        { std::forward<T>(value).isNull() } -> std::same_as<bool>;
    };
template<class T> concept NativeBooleanCast = requires(T&& value) {
    static_cast<bool>(std::forward<T>(value));
};
template<class T> inline constexpr bool sourceBooleanValueReceiver =
    !::nicxlive::core::d_runtime::detail::sourceNativeObjectOrigin<std::remove_cvref_t<T>>
    && (!std::is_polymorphic_v<std::remove_cvref_t<T>> || requires {
        requires (!::nicxlive::core::d_runtime::SourceTypeSpellings<std::remove_cvref_t<T>>::isNativeObject);
    });

template<class T> concept SourceBooleanMethod = sourceBooleanValueReceiver<T>
    && requires(T&& value) {
        requires std::same_as<std::remove_reference_t<
            decltype(std::forward<T>(value).template opCast<bool>())>, bool>;
    };
}

// D reference conditions inspect the descriptor, including nonnull empty arrays.
// Other mapped values use their actual bool-conversion lowering, never operator!.
template<class T> requires (detail::SourceDescriptorTruth<T> || detail::SourceBooleanMethod<T>
    || detail::NativeBooleanCast<T>)
bool ngSourceBool(T&& value) {
    if constexpr (detail::SourceDescriptorTruth<T>) return !std::forward<T>(value).isNull();
    else if constexpr (detail::SourceBooleanMethod<T>) return std::forward<T>(value).template opCast<bool>();
    else return static_cast<bool>(std::forward<T>(value));
}
} // namespace nijigenerate::compat
