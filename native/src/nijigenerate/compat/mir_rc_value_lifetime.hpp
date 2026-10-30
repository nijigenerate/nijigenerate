#pragma once

#include "mir_rc_array.hpp"
#include <math/types.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

namespace nijigenerate::compat {
namespace detail {

// inmath 1.0.6 linalg.d: Vector contains only float[dimension]. There is no
// destructor, postblit or opAssign. Source float is four bytes; these constants
// describe those declarations, rather than deriving Source size from C++.
template<class T, std::int32_t SourceSize> struct MirRCFloatVectorLifetime {
    inline static const MirRCTypeInfo typeInfo{nullptr, SourceSize};
    inline static constexpr bool elaborateAssign = false;
    inline static constexpr bool elaborateDestructor = false;

    // Dynamic int selects this(Args...)(Args), whose construct recursion ends
    // after one coordinate and fails its i == dimension assertion. The float
    // fill constructor requires a conversion and loses the exact int match.
    inline static constexpr bool intEmplaceSupported = false;
    static void emplaceInt(T*, std::int32_t) = delete;
};

} // namespace detail

template<> struct MirRCElementOperations<nicxlive::core::math::Vec2>
    : detail::MirRCFloatVectorLifetime<nicxlive::core::math::Vec2, 8> {
    using T = nicxlive::core::math::Vec2;
    // Guards the existing carrier used by the RC byte-stride provider. These
    // checks do not establish a Source type identity or a borrowed array view.
    static_assert(sizeof(float) == 4 && sizeof(T) == 8 && std::is_standard_layout_v<T>);
    static_assert(offsetof(T, x) == 0 && offsetof(T, y) == 4);
    static T initializer() noexcept {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan};
    }
    static const void* payload() noexcept {
        // Vector is nonzero-initialized and is not itself a floating scalar.
        static const T value = initializer();
        return &value;
    }
    static void fillDefault(T* values, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(values + i, initializer());
    }
    static void assignElement(T& destination, T& source) noexcept {
        destination.x = source.x;
        destination.y = source.y;
    }
};

template<> struct MirRCElementOperations<nicxlive::core::math::Vec3>
    : detail::MirRCFloatVectorLifetime<nicxlive::core::math::Vec3, 12> {
    using T = nicxlive::core::math::Vec3;
    static_assert(sizeof(float) == 4 && sizeof(T) == 12 && std::is_standard_layout_v<T>);
    static_assert(offsetof(T, x) == 0 && offsetof(T, y) == 4 && offsetof(T, z) == 8);
    static T initializer() noexcept {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan};
    }
    static const void* payload() noexcept {
        static const T value = initializer();
        return &value;
    }
    static void fillDefault(T* values, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(values + i, initializer());
    }
    static void assignElement(T& destination, T& source) noexcept {
        destination.x = source.x;
        destination.y = source.y;
        destination.z = source.z;
    }
};

template<> struct MirRCElementOperations<nicxlive::core::math::Vec4>
    : detail::MirRCFloatVectorLifetime<nicxlive::core::math::Vec4, 16> {
    using T = nicxlive::core::math::Vec4;
    static_assert(sizeof(float) == 4 && sizeof(T) == 16 && std::is_standard_layout_v<T>);
    static_assert(offsetof(T, x) == 0 && offsetof(T, y) == 4 && offsetof(T, z) == 8 && offsetof(T, w) == 12);
    static T initializer() noexcept {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan, nan};
    }
    static const void* payload() noexcept {
        static const T value = initializer();
        return &value;
    }
    static void fillDefault(T* values, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(values + i, initializer());
    }
    static void assignElement(T& destination, T& source) noexcept {
        destination.x = source.x;
        destination.y = source.y;
        destination.z = source.z;
        destination.w = source.w;
    }
};

} // namespace nijigenerate::compat
