#pragma once

#include "mir_rc_array.hpp"
#include <nijigenerate/core/cv/contours.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace nijigenerate::compat {
// Source contours.d:41-52 declares two four-int aggregates with no constructor,
// destructor, postblit or assignment hook. T(int) initializes only the first
// field; the remaining fields keep their actual default int initializer, zero.
template<> struct MirRCElementDefinition<::nijigenerate::core::cv::contours::ContourHierarchy> {
    using T = ::nijigenerate::core::cv::contours::ContourHierarchy;
    inline static constexpr std::int32_t sourceByteSize = 16;
    inline static constexpr bool elaborateAssign = false;
    inline static constexpr bool elaborateDestructor = false;
    inline static constexpr bool noMutableIndirections = true;
    inline static constexpr bool sharedValueOperationsEquivalent = true;
    inline static constexpr bool readOnlyValueOperationsEquivalent = true;
    static_assert(sizeof(T) == 16 && std::is_standard_layout_v<T>);
    static_assert(offsetof(T, next) == 0 && offsetof(T, prev) == 4 &&
        offsetof(T, child) == 8 && offsetof(T, parent) == 12);
    static const void* initializerPayload() noexcept { return nullptr; }
    static void fillDefault(T* values, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(values + i, T{0, 0, 0, 0});
    }
    static void emplaceInt(T* value, std::int32_t first) noexcept {
        std::construct_at(value, T{first, 0, 0, 0});
    }
    static void assignElement(T& destination, T& source) noexcept {
        destination.next = source.next;
        destination.prev = source.prev;
        destination.child = source.child;
        destination.parent = source.parent;
    }
};

template<> struct MirRCElementDefinition<::nijigenerate::core::cv::contours::BoundingBox> {
    using T = ::nijigenerate::core::cv::contours::BoundingBox;
    inline static constexpr std::int32_t sourceByteSize = 16;
    inline static constexpr bool elaborateAssign = false;
    inline static constexpr bool elaborateDestructor = false;
    inline static constexpr bool noMutableIndirections = true;
    inline static constexpr bool sharedValueOperationsEquivalent = true;
    inline static constexpr bool readOnlyValueOperationsEquivalent = true;
    static_assert(sizeof(T) == 16 && std::is_standard_layout_v<T>);
    static_assert(offsetof(T, minX) == 0 && offsetof(T, minY) == 4 &&
        offsetof(T, maxX) == 8 && offsetof(T, maxY) == 12);
    static const void* initializerPayload() noexcept { return nullptr; }
    static void fillDefault(T* values, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) std::construct_at(values + i, T{0, 0, 0, 0});
    }
    static void emplaceInt(T* value, std::int32_t first) noexcept {
        std::construct_at(value, T{first, 0, 0, 0});
    }
    static void assignElement(T& destination, T& source) noexcept {
        destination.minX = source.minX;
        destination.minY = source.minY;
        destination.maxX = source.maxX;
        destination.maxY = source.maxY;
    }
};
} // namespace nijigenerate::compat

// Optional defining-type intake: both real type declarations and the actual
// generic converter are visible, without adding contour/task dependencies to
// the independent numeric converter's unconditional include graph.
#include <nijigenerate/core/cv/distance_conversion.hpp>
