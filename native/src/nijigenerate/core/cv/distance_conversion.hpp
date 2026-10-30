#pragma once

#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/mir_rc_array.hpp"
#include "nijigenerate/compat/mir_rc_value_lifetime.hpp"
#include "nijigenerate/compat/mir_rc_integer_vector_lifetime.hpp"
#include "nijigenerate/compat/mir_rc_shared_scalar.hpp"
#include "nijigenerate/compat/mir_rc_immutable_value.hpp"
#include "nijigenerate/compat/mir_rc_qualified_definition.hpp"
#include "distance1d.hpp"
#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace nijigenerate::core::cv::distancetransform {
// A mapped struct supplies its actual Source emplace initialization and
// assignment/constructor body, not a native constructibility acceptance flag.
template<class T, class = void> struct RCArrayEmplaceOperations {
    static void emplaceDimension(T* storage, std::int32_t value) {
        compat::MirRCElementOperations<std::remove_cv_t<T>>::emplaceInt(storage, value);
    }
};

template<class T> struct RCArrayEmplaceOperations<T, std::enable_if_t<
    !std::is_const_v<T> && !std::is_volatile_v<T> &&
    ((std::is_integral_v<T> && sizeof(T) >= sizeof(std::int32_t) && !std::is_same_v<T, bool>) ||
     std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, long double>)>> {
    static void emplaceDimension(T* storage, std::int32_t value) {
        std::construct_at(storage, value);
    }
};

// The Source fallback T(int) uses implicit construction, not cast(T). A dynamic
// int neither narrows to these integral types nor converts to a named enum.
template<class T> struct RCArrayEmplaceOperations<T, std::enable_if_t<
    std::is_enum_v<T> || (std::is_integral_v<T> &&
        (sizeof(T) < sizeof(std::int32_t) || std::is_same_v<T, bool>))>> {
    static void emplaceDimension(T*, std::int32_t) {
        static_assert(std::is_same_v<T, void>,
            "Source emplace cannot implicitly construct this type from a dynamic int");
    }
};

namespace detail {
// Legacy native capability spelling; it does not constrain the Source functions.
template<class T> concept RCValue = !std::is_const_v<T> && !std::is_volatile_v<T> &&
    ((std::is_integral_v<T> && (sizeof(T) == 4 || sizeof(T) == 8)) ||
     std::is_same_v<T, float> || std::is_same_v<T, double>);

template<class T, std::size_t N>
compat::MirRCArray<T> convertValues(std::span<T> input, const std::int32_t (&values)[N]) {
    // Original MIR resolves rcarray!T(args...) as values, not dimensions.
    // An oversized source performs an unchecked raw-pointer write in D.
    if constexpr (std::is_same_v<typename compat::MirRCIntConstruction<std::remove_cv_t<T>>::SourceCase,
        compat::MirRCInvalidSourceIntBody>) {
        compat::MirRCIntConstruction<std::remove_cv_t<T>>::emplace(nullptr, 0);
        return {};
    } else if constexpr (std::is_enum_v<T> || (std::is_integral_v<T> &&
        (sizeof(T) < sizeof(std::int32_t) || std::is_same_v<T, bool>))) {
        RCArrayEmplaceOperations<T>::emplaceDimension(nullptr, 0);
        return {};
    } else {
        using Operations = compat::MirRCElementOperations<std::remove_cv_t<T>>;
        compat::MirRCArray<T> storage(N, Operations::elaborateDestructor);
        if constexpr (!Operations::elaborateAssign && std::same_as<T, std::int32_t>) {
            std::memcpy(storage.ptr(), values, sizeof(values));
        } else {
            for (std::size_t i = 0; i < N; ++i)
                RCArrayEmplaceOperations<T>::emplaceDimension(storage.ptr() + i, values[i]);
        }
        // This is the original unchecked pointer store, including its invalid domain.
        for (std::size_t i = 0; i < input.size(); ++i)
            Operations::assignElement(storage.ptr()[i], input[i]);
        return storage;
    }
}
} // namespace detail

namespace detail {
template<class T, class Qualifier, std::size_t N>
compat::MirRCQualifiedArray<T, Qualifier> convertQualifiedValues(
    compat::MirRCQualifiedSlice<T, Qualifier>& input, const std::int32_t (&values)[N]) {
    using Definition = compat::MirRCQualifiedTypeDefinition<T, Qualifier>;
    auto result = Definition::createIntegerValues(values);
    auto& descriptor = compat::detail::MirRCQualifiedDescriptorAccess::slice(input);
    const auto total = Definition::sliceLength(descriptor);
    for (std::size_t i = 0; i < total; ++i)
        Definition::assignInputElement(result, i, descriptor, i);
    return compat::MirRCQualifiedArray<T, Qualifier>::fromProducedDescriptor(Definition::moveArray(result));
}

template<compat::MirRCPlainSharedValue T, std::size_t N>
compat::SourceSharedValueRCArray<T> convertSharedValues(
    compat::SourceSharedValueSlice<T>& input, const std::int32_t (&values)[N]) {
    auto qualified = compat::MirRCQualifiedSlice<T, compat::MirRCSourceShared>::fromDeclaredDescriptor(input);
    auto result = convertQualifiedValues(qualified, values);
    return compat::detail::MirRCQualifiedDescriptorAccess::takeArray(result);
}

template<class T, std::size_t N>
compat::MirRCArray<T> convertDeclaredValues(compat::Slice<T>& input, const std::int32_t (&values)[N]) {
    auto declared = compat::MirRCQualifiedSlice<T, compat::MirRCSourceUnqualified>::fromDeclaredDescriptor(input);
    auto result = convertQualifiedValues(declared, values);
    return compat::detail::MirRCQualifiedDescriptorAccess::takeArray(result);
}
} // namespace detail

template<class T, class Qualifier>
compat::MirRCQualifiedArray<T, Qualifier> convertToRCArray(
    compat::MirRCQualifiedSlice<T, Qualifier> input, std::int32_t dim0, std::int32_t dim1) {
    const std::int32_t values[]{dim0, dim1};
    return detail::convertQualifiedValues(input, values);
}

template<class T, class Qualifier>
compat::MirRCQualifiedArray<T, Qualifier> convertToRCArray(
    compat::MirRCQualifiedSlice<T, Qualifier> input, std::int32_t dim0, std::int32_t dim1, std::int32_t dim2) {
    const std::int32_t values[]{dim0, dim1, dim2};
    return detail::convertQualifiedValues(input, values);
}

template<compat::MirRCPlainSharedValue T>
compat::SourceSharedValueRCArray<T> convertToRCArray(
    compat::SourceSharedValueSlice<T> input, std::int32_t dim0, std::int32_t dim1) {
    const std::int32_t values[]{dim0, dim1};
    return detail::convertSharedValues(input, values);
}

template<compat::MirRCPlainSharedValue T>
compat::SourceSharedValueRCArray<T> convertToRCArray(
    compat::SourceSharedValueSlice<T> input, std::int32_t dim0, std::int32_t dim1, std::int32_t dim2) {
    const std::int32_t values[]{dim0, dim1, dim2};
    return detail::convertSharedValues(input, values);
}

// The original assignment through A.ptr cannot mutate immutable elements.
template<compat::MirRCPlainSharedValue T> requires compat::MirRCPlainReadOnlyValue<T>
compat::SourceSharedConstValueRCArray<T> convertToRCArray(
    compat::SourceSharedConstValueSlice<T>, std::int32_t, std::int32_t) = delete;
template<compat::MirRCPlainSharedValue T> requires compat::MirRCPlainReadOnlyValue<T>
compat::SourceSharedConstValueRCArray<T> convertToRCArray(
    compat::SourceSharedConstValueSlice<T>, std::int32_t, std::int32_t, std::int32_t) = delete;
template<compat::MirRCPlainReadOnlyValue T>
compat::SourceImmutableValueRCArray<T> convertToRCArray(
    compat::SourceImmutableValueSlice<T>, std::int32_t, std::int32_t) = delete;
template<compat::MirRCPlainReadOnlyValue T>
compat::SourceImmutableValueRCArray<T> convertToRCArray(
    compat::SourceImmutableValueSlice<T>, std::int32_t, std::int32_t, std::int32_t) = delete;

// Source T[] parameters retain their actual backing throughout element callbacks.
template<class T>
compat::MirRCArray<T> convertToRCArray(compat::Slice<T> input, std::int32_t dim0, std::int32_t dim1) {
    const std::int32_t values[]{dim0, dim1};
    return detail::convertDeclaredValues(input, values);
}

template<class T>
compat::MirRCArray<T> convertToRCArray(compat::Slice<T> input, std::int32_t dim0,
    std::int32_t dim1, std::int32_t dim2) {
    const std::int32_t values[]{dim0, dim1, dim2};
    return detail::convertDeclaredValues(input, values);
}

// Native interoperability: the caller retains the span's actual backing.
template<class T>
compat::MirRCArray<T> convertToRCArray(std::span<T> input, std::int32_t dim0, std::int32_t dim1) {
    const std::int32_t values[]{dim0, dim1};
    return detail::convertValues(input, values);
}

template<class T>
compat::MirRCArray<T> convertToRCArray(std::span<T> input, std::int32_t dim0,
    std::int32_t dim1, std::int32_t dim2) {
    const std::int32_t values[]{dim0, dim1, dim2};
    return detail::convertValues(input, values);
}
} // namespace nijigenerate::core::cv::distancetransform
