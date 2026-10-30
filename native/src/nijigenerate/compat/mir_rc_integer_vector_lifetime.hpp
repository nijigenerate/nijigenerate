#pragma once

#include "mir_rc_array.hpp"
#include <math/types.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace nijigenerate::compat {
// inmath 1.0.6 Vector!(int, N): int[N] storage, zero initializer,
// no postblit, custom assignment or destructor. The fixed int fill constructor
// wins the exact-match tie against the variadic constructor for an int argument.
template<> struct MirRCElementDefinition<nicxlive::core::math::Vec2i> {
    using T = nicxlive::core::math::Vec2i;
    inline static constexpr std::int32_t sourceByteSize = 8;
    inline static constexpr bool elaborateAssign = false;
    inline static constexpr bool elaborateDestructor = false;
    static_assert(sizeof(std::int32_t) == 4 && sizeof(T) == 8 && std::is_standard_layout_v<T>);
    static_assert(offsetof(T, x) == 0 && offsetof(T, y) == 4);
    static const void* initializerPayload() noexcept { return nullptr; }
    static void fillDefault(T* values, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(values + i, T{0, 0});
    }
    static void emplaceInt(T* value, std::int32_t scalar) noexcept {
        std::construct_at(value, T{scalar, scalar});
    }
    static void assignElement(T& destination, T& source) noexcept {
        destination.x = source.x;
        destination.y = source.y;
    }
};

template<> struct MirRCElementDefinition<nicxlive::core::math::Vec4i> {
    using T = nicxlive::core::math::Vec4i;
    inline static constexpr std::int32_t sourceByteSize = 16;
    inline static constexpr bool elaborateAssign = false;
    inline static constexpr bool elaborateDestructor = false;
    static_assert(sizeof(std::int32_t) == 4 && sizeof(T) == 16 && std::is_standard_layout_v<T>);
    static_assert(offsetof(T, x) == 0 && offsetof(T, y) == 4 && offsetof(T, z) == 8 && offsetof(T, w) == 12);
    static const void* initializerPayload() noexcept { return nullptr; }
    static void fillDefault(T* values, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(values + i, T{0, 0, 0, 0});
    }
    static void emplaceInt(T* value, std::int32_t scalar) noexcept {
        std::construct_at(value, T{scalar, scalar, scalar, scalar});
    }
    static void assignElement(T& destination, T& source) noexcept {
        destination.x = source.x;
        destination.y = source.y;
        destination.z = source.z;
        destination.w = source.w;
    }
};
} // namespace nijigenerate::compat
